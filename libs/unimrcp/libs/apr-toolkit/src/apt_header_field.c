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
 * $Id: apt_header_field.c 2223 2014-11-12 00:37:40Z achaloyan@gmail.com $
 */

#include "apt_header_field.h"
#include "apt_text_stream.h"

#define UNKNOWN_HEADER_FIELD_ID (apr_size_t)-1

/** Allocate an empty header field */
APT_DECLARE(apt_header_field_t*) apt_header_field_alloc(apr_pool_t *pool)
{
	apt_header_field_t *header_field = apr_palloc(pool,sizeof(apt_header_field_t));
	apt_string_reset(&header_field->name);
	apt_string_reset(&header_field->value);
	header_field->id = UNKNOWN_HEADER_FIELD_ID;
	APR_RING_ELEM_INIT(header_field,link);
	return header_field;
}

/** Create a header field using given name and value APT strings */
APT_DECLARE(apt_header_field_t*) apt_header_field_create(const apt_str_t *name, const apt_str_t *value, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	if(!name || !value) {
		return NULL;
	}
	header_field = apr_palloc(pool,sizeof(apt_header_field_t));
	apt_string_copy(&header_field->name,name,pool);
	apt_string_copy(&header_field->value,value,pool);
	header_field->id = UNKNOWN_HEADER_FIELD_ID;
	APR_RING_ELEM_INIT(header_field,link);
	return header_field;
}

/** Create a header field using given name and value C strings */
APT_DECLARE(apt_header_field_t*) apt_header_field_create_c(const char *name, const char *value, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	if(!name || !value) {
		return NULL;
	}
	header_field = apr_palloc(pool,sizeof(apt_header_field_t));
	apt_string_assign(&header_field->name,name,pool);
	apt_string_assign(&header_field->value,value,pool);
	header_field->id = UNKNOWN_HEADER_FIELD_ID;
	APR_RING_ELEM_INIT(header_field,link);
	return header_field;
}

/* Create a header field from entire text line consisting of a name and value pair */
APT_DECLARE(apt_header_field_t*) apt_header_field_create_from_line(const apt_str_t *line, char separator, apr_pool_t *pool)
{
	apt_str_t item;
	apt_text_stream_t stream;
	apt_header_field_t *header_field;
	if(!line) {
		return NULL;
	}
	
	header_field = apr_palloc(pool,sizeof(apt_header_field_t));
	stream.text = *line;
	apt_text_stream_reset(&stream);

	/* read name */
	if(apt_text_field_read(&stream,separator,TRUE,&item) == FALSE) {
		return NULL;
	}
	apt_string_copy(&header_field->name,&item,pool);

	/* read value */
	if(apt_text_field_read(&stream,0,TRUE,&item) == TRUE) {
		apt_string_copy(&header_field->value,&item,pool);
	}
	else {
		apt_string_reset(&header_field->value);
	}

	header_field->id = UNKNOWN_HEADER_FIELD_ID;
	APR_RING_ELEM_INIT(header_field,link);
	return header_field;
}

/** Copy specified header field */
APT_DECLARE(apt_header_field_t*) apt_header_field_copy(const apt_header_field_t *src_header_field, apr_pool_t *pool)
{
	apt_header_field_t *header_field = apr_palloc(pool,sizeof(apt_header_field_t));
	apt_string_copy(&header_field->name,&src_header_field->name,pool);
	apt_string_copy(&header_field->value,&src_header_field->value,pool);
	header_field->id = src_header_field->id;
	APR_RING_ELEM_INIT(header_field,link);
	return header_field;
}

/** Initialize header section (collection of header fields) */
APT_DECLARE(void) apt_header_section_init(apt_header_section_t *header)
{
	APR_RING_INIT(&header->ring, apt_header_field_t, link);
	header->arr = NULL;
	header->arr_size = 0;
}

/** Allocate header section to set/get header fields by numeric identifiers */
APT_DECLARE(apt_bool_t) apt_header_section_array_alloc(apt_header_section_t *header, apr_size_t max_field_count, apr_pool_t *pool)
{
	if(!max_field_count) {
		return FALSE;
	}

	header->arr = (apt_header_field_t**)apr_pcalloc(pool,sizeof(apt_header_field_t*) * max_field_count);
	header->arr_size = max_field_count;
	return TRUE;
}

/** Add (append) header field to header section */
APT_DECLARE(apt_bool_t) apt_header_section_field_add(apt_header_section_t *header, apt_header_field_t *header_field)
{
	if(header_field->id < header->arr_size) {
		if(header->arr[header_field->id]) {
			return FALSE;
		}
		header->arr[header_field->id] = header_field;
	}
	APR_RING_INSERT_TAIL(&header->ring,header_field,apt_header_field_t,link);
	return TRUE;
}

/** Insert header field to header section based on numreic identifier if specified */
APT_DECLARE(apt_bool_t) apt_header_section_field_insert(apt_header_section_t *header, apt_header_field_t *header_field)
{
	apt_header_field_t *it;
	if(header_field->id < header->arr_size) {
		if(header->arr[header_field->id]) {
			return FALSE;
		}
		header->arr[header_field->id] = header_field;

		for(it = APR_RING_FIRST(&header->ring);
				it != APR_RING_SENTINEL(&header->ring, apt_header_field_t, link);
					it = APR_RING_NEXT(it, link)) {
			if(header_field->id < it->id) {
				APR_RING_INSERT_BEFORE(it,header_field,link);
				return TRUE;
			}
		}
	}

	APR_RING_INSERT_TAIL(&header->ring,header_field,apt_header_field_t,link);
	return TRUE;
}

/** Set header field in the array of header fields using associated numeric identifier */
APT_DECLARE(apt_bool_t) apt_header_section_field_set(apt_header_section_t *header, apt_header_field_t *header_field)
{
	if(header_field->id >= header->arr_size) {
		return FALSE;
	}
	if(header->arr[header_field->id]) {
		return FALSE;
	}
	header->arr[header_field->id] = header_field;
	return TRUE;
}

/** Remove header field from header section */
APT_DECLARE(apt_bool_t) apt_header_section_field_remove(apt_header_section_t *header, apt_header_field_t *header_field)
{
	if(header_field->id < header->arr_size) {
		header->arr[header_field->id] = NULL;
	}
	APR_RING_REMOVE(header_field,link);
	return TRUE;
}
