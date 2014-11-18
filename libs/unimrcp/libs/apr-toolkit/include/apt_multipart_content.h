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
 * $Id: apt_multipart_content.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef APT_MULTIPART_CONTENT_H
#define APT_MULTIPART_CONTENT_H

/**
 * @file apt_multipart_content.h
 * @brief Multipart Content Routine
 */ 

#include "apt_header_field.h"

APT_BEGIN_EXTERN_C

/** Opaque multipart content declaration */
typedef struct apt_multipart_content_t apt_multipart_content_t;

/** Content part declaration */
typedef struct apt_content_part_t apt_content_part_t;

/** Content part */
struct apt_content_part_t {
	/** Header section */
	apt_header_section_t header;
	/** Body */
	apt_str_t            body;

	/** Pointer to parsed content-type header field */
	apt_str_t           *type;
	/** Pointer to parsed content-id header field */
	apt_str_t           *id;
	/** Pointer to parsed content-length header field */
	apt_str_t           *length;
};

/**
 * Create an empty multipart content
 * @param max_content_size the max size of the content (body)
 * @param boundary the boundary to separate content parts
 * @param pool the pool to allocate memory from
 * @return an empty multipart content
 */
APT_DECLARE(apt_multipart_content_t*) apt_multipart_content_create(apr_size_t max_content_size, const apt_str_t *boundary, apr_pool_t *pool);

/** 
 * Add content part to multipart content
 * @param multipart_content the multipart content to add content part to
 * @param content_part the content part to add
 * @return TRUE on success
 */
APT_DECLARE(apt_bool_t) apt_multipart_content_add(apt_multipart_content_t *multipart_content, const apt_content_part_t *content_part);

/** 
 * Add content part to multipart content by specified header fields and body
 * @param multipart_content the multipart content to add content part to
 * @param content_type the type of content part
 * @param content_id the identifier of content part
 * @param body the body of content part
 * @return TRUE on success
 */
APT_DECLARE(apt_bool_t) apt_multipart_content_add2(apt_multipart_content_t *multipart_content, const apt_str_t *content_type, const apt_str_t *content_id, const apt_str_t *body);

/** 
 * Finalize multipart content generation 
 * @param multipart_content the multipart content to finalize
 * @return generated multipart content
 */
APT_DECLARE(apt_str_t*) apt_multipart_content_finalize(apt_multipart_content_t *multipart_content);


/** 
 * Assign body to multipart content to get (parse) each content part from
 * @param body the body of multipart content to parse
 * @param boundary the boundary to separate content parts
 * @param pool the pool to allocate memory from
 * @return multipart content with assigned body
 */
APT_DECLARE(apt_multipart_content_t*) apt_multipart_content_assign(const apt_str_t *body, const apt_str_t *boundary, apr_pool_t *pool);

/** 
 * Get the next content part
 * @param multipart_content the multipart content to get the next content part from
 * @param content_part the parsed content part
 * @param is_final indicates the final boundary is reached
 * @return TRUE on success
 */
APT_DECLARE(apt_bool_t) apt_multipart_content_get(apt_multipart_content_t *multipart_content, apt_content_part_t *content_part, apt_bool_t *is_final);


APT_END_EXTERN_C

#endif /* APT_MULTIPART_CONTENT_H */
