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

#include "mrcp_header.h"
#include "mrcp_generic_header.h"
#include "apt_text_message.h"
#include "apt_log.h"

#define MRCP_CHANNEL_ID         "Channel-Identifier"
#define MRCP_CHANNEL_ID_LENGTH  (sizeof(MRCP_CHANNEL_ID)-1)


/** Allocate MRCP message-header data */
MRCP_DECLARE(apt_bool_t) mrcp_message_header_data_alloc(
						mrcp_message_header_t *header,
						const mrcp_header_vtable_t *generic_header_vtable,
						const mrcp_header_vtable_t *resource_header_vtable,
						apr_pool_t *pool)
{
	if(!generic_header_vtable || !resource_header_vtable) {
		return FALSE;
	}

	header->generic_header_accessor.data = NULL;
	header->generic_header_accessor.vtable = generic_header_vtable;

	header->resource_header_accessor.data = NULL;
	header->resource_header_accessor.vtable = resource_header_vtable;

	apt_header_section_array_alloc(
		&header->header_section,
		header->generic_header_accessor.vtable->field_count +
		header->resource_header_accessor.vtable->field_count,
		pool);

	mrcp_header_allocate(&header->generic_header_accessor,pool);
	mrcp_header_allocate(&header->resource_header_accessor,pool);
	return TRUE;
}

MRCP_DECLARE(mrcp_message_header_t*) mrcp_message_header_create(
						const mrcp_header_vtable_t *generic_header_vtable,
						const mrcp_header_vtable_t *resource_header_vtable,
						apr_pool_t *pool)
{
	mrcp_message_header_t *header = apr_palloc(pool,sizeof(mrcp_message_header_t));
	apt_header_section_init(&header->header_section);
	mrcp_message_header_data_alloc(header,generic_header_vtable,resource_header_vtable,pool);
	return header;
}

/** Add MRCP header field */
MRCP_DECLARE(apt_bool_t) mrcp_header_field_add(mrcp_message_header_t *header, apt_header_field_t *header_field, apr_pool_t *pool)
{
	apt_bool_t status = FALSE;
	if(apt_string_is_empty(&header_field->name) == FALSE) {
		/* normal header */
		if(mrcp_header_field_value_parse(&header->resource_header_accessor,header_field,pool) == TRUE) {
			header_field->id += GENERIC_HEADER_COUNT;
		}
		else if(mrcp_header_field_value_parse(&header->generic_header_accessor,header_field,pool) == TRUE) {
		}
		else { 
			apt_log(APT_LOG_MARK,APT_PRIO_DEBUG,"Unknown MRCP header field: %s",header_field->name.buf);
		}
		status = apt_header_section_field_add(&header->header_section,header_field);
	}
	return status;
}

/** Parse MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_parse(mrcp_message_header_t *header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	for(header_field = APR_RING_FIRST(&header->header_section.ring);
			header_field != APR_RING_SENTINEL(&header->header_section.ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {

		if(mrcp_header_field_value_parse(&header->resource_header_accessor,header_field,pool) == TRUE) {
			header_field->id += GENERIC_HEADER_COUNT;
			apt_header_section_field_set(&header->header_section,header_field);
		}
		else if(mrcp_header_field_value_parse(&header->generic_header_accessor,header_field,pool) == TRUE) {
			apt_header_section_field_set(&header->header_section,header_field);
		}
		else {
			apt_log(APT_LOG_MARK,APT_PRIO_WARNING,"Unknown MRCP header field: %s",header_field->name.buf);
		}
	}

	return TRUE;
}

static apt_bool_t mrcp_header_accessor_value_duplicate(mrcp_message_header_t *header, apt_header_field_t *header_field,
											  const mrcp_message_header_t *src_header, const apt_header_field_t *src_header_field, 
											  apr_pool_t *pool)
{
	apt_bool_t status = FALSE;
	if(header_field->id < GENERIC_HEADER_COUNT) {
		status = mrcp_header_field_value_duplicate(
			&header->generic_header_accessor,
			&src_header->generic_header_accessor,
			header_field->id,
			&header_field->value,
			pool);
	}
	else {
		status = mrcp_header_field_value_duplicate(
			&header->resource_header_accessor,
			&src_header->resource_header_accessor,
			header_field->id - GENERIC_HEADER_COUNT,
			&header_field->value,
			pool);
	}
	return status;
}

/** Set (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_set(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_header_field_t *src_header_field;
	for(src_header_field = APR_RING_FIRST(&src_header->header_section.ring);
			src_header_field != APR_RING_SENTINEL(&src_header->header_section.ring, apt_header_field_t, link);
				src_header_field = APR_RING_NEXT(src_header_field, link)) {

		header_field = apt_header_section_field_get(&header->header_section,src_header_field->id);
		if(header_field) {
			/* this header field has already been set, just copy its value */
			apt_string_copy(&header_field->value,&src_header_field->value,pool);
		}
		else {
			/* copy the entire header field and add it to the header section */
			header_field = apt_header_field_copy(src_header_field,pool);
			apt_header_section_field_add(&header->header_section,header_field);
		}

		mrcp_header_accessor_value_duplicate(header,header_field,src_header,src_header_field,pool);
	}

	return TRUE;
}

/** Get (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_get(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, const mrcp_message_header_t *mask_header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_header_field_t *src_header_field;
	const apt_header_field_t *mask_header_field;
	for(mask_header_field = APR_RING_FIRST(&mask_header->header_section.ring);
			mask_header_field != APR_RING_SENTINEL(&mask_header->header_section.ring, apt_header_field_t, link);
				mask_header_field = APR_RING_NEXT(mask_header_field, link)) {

		header_field = apt_header_section_field_get(&header->header_section,mask_header_field->id);
		if(header_field) {
			/* this header field has already been set, skip to the next one */
			continue;
		}

		src_header_field = apt_header_section_field_get(&src_header->header_section,mask_header_field->id);
		if(src_header_field) {
			/* copy the entire header field */
			header_field = apt_header_field_copy(src_header_field,pool);
			mrcp_header_accessor_value_duplicate(header,header_field,src_header,src_header_field,pool);
		}
		else {
			/* copy only the name of the header field */
			header_field = apt_header_field_copy(mask_header_field,pool);
		}
		/* add the header field to the header section */
		apt_header_section_field_add(&header->header_section,header_field);
	}

	return TRUE;
}

/** Inherit (copy) MRCP header fields */
MRCP_DECLARE(apt_bool_t) mrcp_header_fields_inherit(mrcp_message_header_t *header, const mrcp_message_header_t *src_header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	const apt_header_field_t *src_header_field;
	for(src_header_field = APR_RING_FIRST(&src_header->header_section.ring);
			src_header_field != APR_RING_SENTINEL(&src_header->header_section.ring, apt_header_field_t, link);
				src_header_field = APR_RING_NEXT(src_header_field, link)) {

		header_field = apt_header_section_field_get(&header->header_section,src_header_field->id);
		if(header_field) {
			/* this header field has already been set, skip to the next one */
			continue;
		}

		/* copy the entire header field and add it to the header section */
		header_field = apt_header_field_copy(src_header_field,pool);
		mrcp_header_accessor_value_duplicate(header,header_field,src_header,src_header_field,pool);
		apt_header_section_field_add(&header->header_section,header_field);
	}
	return TRUE;
}


/** Initialize MRCP channel-identifier */
MRCP_DECLARE(void) mrcp_channel_id_init(mrcp_channel_id *channel_id)
{
	apt_string_reset(&channel_id->session_id);
	apt_string_reset(&channel_id->resource_name);
}

/** Parse MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_parse(mrcp_channel_id *channel_id, mrcp_message_header_t *header, apr_pool_t *pool)
{
	apt_header_field_t *header_field;
	for(header_field = APR_RING_FIRST(&header->header_section.ring);
			header_field != APR_RING_SENTINEL(&header->header_section.ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {

		if(header_field->value.length && strncasecmp(header_field->name.buf,MRCP_CHANNEL_ID,MRCP_CHANNEL_ID_LENGTH) == 0) {
			apt_id_resource_parse(&header_field->value,'@',&channel_id->session_id,&channel_id->resource_name,pool);
			apt_header_section_field_remove(&header->header_section,header_field);
			return TRUE;
		}
	}
	return FALSE;
}

/** Generate MRCP channel-identifier */
MRCP_DECLARE(apt_bool_t) mrcp_channel_id_generate(mrcp_channel_id *channel_id, apt_text_stream_t *stream)
{
	apt_str_t *str;
	char *pos = stream->pos;
	if(pos + MRCP_CHANNEL_ID_LENGTH + 2 + channel_id->session_id.length + 1 + 
		channel_id->resource_name.length >= stream->end) {
		return FALSE;
	}
	memcpy(pos,MRCP_CHANNEL_ID,MRCP_CHANNEL_ID_LENGTH);
	pos += MRCP_CHANNEL_ID_LENGTH;
	*pos++ = ':';
	*pos++ = APT_TOKEN_SP;
		
	str = &channel_id->session_id;
	memcpy(pos,str->buf,str->length);
	pos += str->length;
	*pos++ = '@';

	str = &channel_id->resource_name;
	memcpy(pos,str->buf,str->length);
	pos += str->length;

	stream->pos = pos;
	return apt_text_eol_insert(stream);
}
