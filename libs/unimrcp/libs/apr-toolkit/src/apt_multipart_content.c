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

#include <stdlib.h>
#include "apt_multipart_content.h"
#include "apt_text_stream.h"
#include "apt_text_message.h"

#define CONTENT_LENGTH_HEADER "Content-Length"
#define CONTENT_TYPE_HEADER   "Content-Type"
#define CONTENT_ID_HEADER     "Content-Id"

#define DEFAULT_BOUNDARY      "break"
#define DEFAULT_HYPHENS        "--"

#define DEFAULT_MULTIPART_CONTENT_SIZE 4096

/** Multipart content */
struct apt_multipart_content_t {
	apr_pool_t       *pool;
	apt_text_stream_t stream;

	apt_str_t         boundary;
	apt_str_t         hyphens;
};

/** Create an empty multipart content */
APT_DECLARE(apt_multipart_content_t*) apt_multipart_content_create(apr_size_t max_content_size, const apt_str_t *boundary, apr_pool_t *pool)
{
	char *buffer;
	apt_multipart_content_t *multipart_content = apr_palloc(pool,sizeof(apt_multipart_content_t));
	multipart_content->pool = pool;

	if(max_content_size == 0) {
		max_content_size = DEFAULT_MULTIPART_CONTENT_SIZE;
	}

	if(boundary) {
		multipart_content->boundary = *boundary;
	}
	else {
		multipart_content->boundary.buf = DEFAULT_BOUNDARY;
		multipart_content->boundary.length = sizeof(DEFAULT_BOUNDARY)-1;
	}

	multipart_content->hyphens.buf = DEFAULT_HYPHENS;
	multipart_content->hyphens.length = sizeof(DEFAULT_HYPHENS)-1;

	buffer = apr_palloc(pool,max_content_size+1);
	apt_text_stream_init(&multipart_content->stream,buffer,max_content_size);
	return multipart_content;
}

/** Initialize content part generation */
static apt_bool_t apt_multipart_content_initialize(apt_multipart_content_t *multipart_content)
{
	/* insert preceding end-of-line */
	if(apt_text_eol_insert(&multipart_content->stream) == FALSE) {
		return FALSE;
	}
	/* insert hyphens */
	if(apt_text_string_insert(&multipart_content->stream,&multipart_content->hyphens) == FALSE) {
		return FALSE;
	}
	/* insert boundary */
	if(apt_text_string_insert(&multipart_content->stream,&multipart_content->boundary) == FALSE) {
		return FALSE;
	}
	return apt_text_eol_insert(&multipart_content->stream);
}

/** Add content part to multipart content */
APT_DECLARE(apt_bool_t) apt_multipart_content_add(apt_multipart_content_t *multipart_content, const apt_content_part_t *content_part)
{
	if(!content_part) {
		return FALSE;
	}

	/* insert preceding eol, hyppens and boudnary */
	if(apt_multipart_content_initialize(multipart_content) == FALSE) {
		return FALSE;
	}

	/* insert header fields */
	if(apt_header_section_generate(&content_part->header,&multipart_content->stream) == FALSE) {
		return FALSE;
	}

	/* insert body */
	return apt_text_string_insert(&multipart_content->stream,&content_part->body);
}

/** Add content part to multipart content by specified header fields and body */
APT_DECLARE(apt_bool_t) apt_multipart_content_add2(apt_multipart_content_t *multipart_content, const apt_str_t *content_type, const apt_str_t *content_id, const apt_str_t *body)
{
	/* insert preceding eol, hyppens and boudnary */
	if(apt_multipart_content_initialize(multipart_content) == FALSE) {
		return FALSE;
	}

	/* insert content-type */
	if(content_type) {
		apt_str_t name = {CONTENT_TYPE_HEADER,sizeof(CONTENT_TYPE_HEADER)-1};
		if(apt_text_name_value_insert(&multipart_content->stream,&name,content_type) == FALSE) {
			return FALSE;
		}
	}

	/* insert content-id */
	if(content_id) {
		apt_str_t name = {CONTENT_ID_HEADER,sizeof(CONTENT_ID_HEADER)-1};
		if(apt_text_name_value_insert(&multipart_content->stream,&name,content_id) == FALSE) {
			return FALSE;
		}
	}

	/* insert content-length */
	if(body) {
		apt_str_t name = {CONTENT_LENGTH_HEADER,sizeof(CONTENT_LENGTH_HEADER)-1};
		if(apt_text_header_name_insert(&multipart_content->stream,&name) == FALSE) {
			return FALSE;
		}
		if(apt_text_size_value_insert(&multipart_content->stream,body->length) == FALSE) {
			return FALSE;
		}
		if(apt_text_eol_insert(&multipart_content->stream) == FALSE) {
			return FALSE;
		}
	}

	/* insert empty line */
	if(apt_text_eol_insert(&multipart_content->stream) == FALSE) {
		return FALSE;
	}

	/* insert body */
	if(body) {
		if(apt_text_string_insert(&multipart_content->stream,body) == FALSE) {
			return FALSE;
		}
	}
	return TRUE;
}

/** Finalize multipart content generation */
APT_DECLARE(apt_str_t*) apt_multipart_content_finalize(apt_multipart_content_t *multipart_content)
{
	apt_text_stream_t *stream = &multipart_content->stream;
	/* insert preceding end-of-line */
	if(apt_text_eol_insert(&multipart_content->stream) == FALSE) {
		return NULL;
	}
	/* insert hyphens */
	if(apt_text_string_insert(&multipart_content->stream,&multipart_content->hyphens) == FALSE) {
		return NULL;
	}
	/* insert boundary */
	if(apt_text_string_insert(&multipart_content->stream,&multipart_content->boundary) == FALSE) {
		return NULL;
	}
	/* insert final hyphens */
	if(apt_text_string_insert(&multipart_content->stream,&multipart_content->hyphens) == FALSE) {
		return NULL;
	}
	if(apt_text_eol_insert(&multipart_content->stream) == FALSE) {
		return NULL;
	}

	stream->text.length = stream->pos - stream->text.buf;
	stream->text.buf[stream->text.length] = '\0';
	return &stream->text;
}


/** Assign body to multipart content to get (parse) each content part from */
APT_DECLARE(apt_multipart_content_t*) apt_multipart_content_assign(const apt_str_t *body, const apt_str_t *boundary, apr_pool_t *pool)
{
	apt_multipart_content_t *multipart_content = apr_palloc(pool,sizeof(apt_multipart_content_t));
	multipart_content->pool = pool;

	if(!body) {
		return FALSE;
	}

	if(boundary) {
		multipart_content->boundary = *boundary;
	}
	else {
		apt_string_reset(&multipart_content->boundary);
	}

	apt_string_reset(&multipart_content->hyphens);
	apt_text_stream_init(&multipart_content->stream,body->buf,body->length);
	return multipart_content;
}

static APR_INLINE void apt_content_part_reset(apt_content_part_t *content_part)
{
	apt_header_section_init(&content_part->header);
	apt_string_reset(&content_part->body);
	content_part->type = NULL;
	content_part->id = NULL;
	content_part->length = NULL;
}

/** Get the next content part */
APT_DECLARE(apt_bool_t) apt_multipart_content_get(apt_multipart_content_t *multipart_content, apt_content_part_t *content_part, apt_bool_t *is_final)
{
	apt_str_t boundary;
	apt_header_field_t *header_field;
	apt_text_stream_t *stream = &multipart_content->stream;

	if(!content_part || !is_final) {
		return FALSE;
	}
	*is_final = FALSE;
	apt_content_part_reset(content_part);

	/* skip preamble */
	apt_text_skip_to_char(stream,'-');
	if(apt_text_is_eos(stream) == TRUE) {
		return FALSE;
	}

	/* skip initial hyphens */
	apt_text_chars_skip(stream,'-');
	if(apt_text_is_eos(stream) == TRUE) {
		return FALSE;
	}

	/* read line and the boundary */
	if(apt_text_line_read(stream,&boundary) == FALSE) {
		return FALSE;
	}

	/* remove optional trailing spaces */
	while(boundary.length && boundary.buf[boundary.length-1] == APT_TOKEN_SP) boundary.length--;

	/* check whether this is the final boundary */
	if(boundary.length >= 2) {
		if(boundary.buf[boundary.length-1] == '-' && boundary.buf[boundary.length-2] == '-') {
			/* final boundary */
			boundary.length -= 2;
			*is_final = TRUE;
		}
	}

	/* compare boundaries */
	if(apt_string_is_empty(&multipart_content->boundary) == TRUE) {
		/* no boundary was specified from user space, 
		learn boundary from the content */
		multipart_content->boundary = boundary;
	}
	else {
		if(apt_string_compare(&multipart_content->boundary,&boundary) == FALSE) {
			/* invalid boundary */
			return FALSE;
		}
	}

	if(*is_final == TRUE) {
		/* final boundary => return TRUE, content remains empty */
		return TRUE;
	}

	/* read header fields */
	if(apt_header_section_parse(&content_part->header,stream,multipart_content->pool) == FALSE) {
		return FALSE;
	}
	
	for(header_field = APR_RING_FIRST(&content_part->header.ring);
			header_field != APR_RING_SENTINEL(&content_part->header.ring, apt_header_field_t, link);
				header_field = APR_RING_NEXT(header_field, link)) {
		if(strncmp(header_field->name.buf,CONTENT_LENGTH_HEADER,header_field->name.length) == 0) {
			content_part->length = &header_field->value;
		}
		else if(strncmp(header_field->name.buf,CONTENT_TYPE_HEADER,header_field->name.length) == 0) {
			content_part->type = &header_field->value;
		}
		else if(strncmp(header_field->name.buf,CONTENT_ID_HEADER,header_field->name.length) == 0) {
			content_part->id = &header_field->value;
		}
	}

	if(content_part->length && apt_string_is_empty(content_part->length) == FALSE) {
		apr_size_t length = atoi(content_part->length->buf);
		if(length + stream->pos > stream->end) {
			return FALSE;
		}

		/* read content */
		apt_string_assign_n(&content_part->body,stream->pos,length,multipart_content->pool);
		stream->pos += length;
	}

	return TRUE;
}
