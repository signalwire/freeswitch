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
 * $Id: apt_string.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef APT_STRING_H
#define APT_STRING_H

/**
 * @file apt_string.h
 * @brief String Representation
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C

/** Empty string */
#define APT_EMPTY_STRING ""

/** String declaration */
typedef struct apt_str_t apt_str_t;

/** String representation */
struct apt_str_t {
	/** String buffer (might be not NULL terminated) */
	char *buf;
	/** Length of the string (not counting terminating NULL character if exists) */
	apr_size_t  length;
}; 

/** Reset string. */
static APR_INLINE void apt_string_reset(apt_str_t *str)
{
	str->buf = NULL;
	str->length = 0;
}

/** Get string buffer. */
static APR_INLINE const char* apt_string_buffer_get(const apt_str_t *str)
{
	if(str->buf) {
		return str->buf;
	}
	return APT_EMPTY_STRING;
}

/** Get string length. */
static APR_INLINE apr_size_t apt_string_length_get(const apt_str_t *str)
{
	return str->length;
}

/** Check whether string is empty. */
static APR_INLINE apr_size_t apt_string_is_empty(const apt_str_t *str)
{
	return str->length ? FALSE : TRUE;
}

/**
 * Set NULL terminated string. 
 * @param str the destination string
 * @param src the NULL terminated string to set
 */
static APR_INLINE void apt_string_set(apt_str_t *str, const char *src)
{
	str->buf = (char*)src;
	str->length = src ? strlen(src) : 0;
}

/**
 * Assign (copy) NULL terminated string. 
 * @param str the destination string
 * @param src the NULL terminated string to copy
 * @param pool the pool to allocate memory from
 */
static APR_INLINE void apt_string_assign(apt_str_t *str, const char *src, apr_pool_t *pool)
{
	str->buf = NULL;
	str->length = src ? strlen(src) : 0;
	if(str->length) {
		str->buf = apr_pstrmemdup(pool,src,str->length);
	}
}

/**
 * Assign (copy) n characters from the src string. 
 * @param str the destination string
 * @param src the NULL terminated string to copy
 * @param pool the pool to allocate memory from
 */
static APR_INLINE void apt_string_assign_n(apt_str_t *str, const char *src, apr_size_t length, apr_pool_t *pool)
{
	str->buf = NULL;
	str->length = length;
	if(str->length) {
		str->buf = apr_pstrmemdup(pool,src,str->length);
	}
}

/**
 * Copy string. 
 * @param dest_str the destination string
 * @param src_str the source string
 * @param pool the pool to allocate memory from
 */
static APR_INLINE void apt_string_copy(apt_str_t *str, const apt_str_t *src_str, apr_pool_t *pool)
{
	str->buf = NULL;
	str->length = src_str->length;
	if(str->length) {
		str->buf = apr_pstrmemdup(pool,src_str->buf,src_str->length);
	}
}

/**
 * Compare two strings (case insensitive). 
 * @param str1 the string to compare
 * @param str2 the string to compare
 * @return TRUE if equal, FALSE otherwise
 */
static APR_INLINE apt_bool_t apt_string_compare(const apt_str_t *str1, const apt_str_t *str2)
{
	if(str1->length != str2->length || !str1->length) {
		return FALSE;
	}
	return (strncasecmp(str1->buf,str2->buf,str1->length) == 0) ? TRUE : FALSE;
}

/**
 * Represent string as iovec. 
 * @param str the string to represent
 * @param vec the iovec to set
 */
static APR_INLINE void apt_string_to_iovec(const apt_str_t *str, struct iovec *vec)
{
	vec->iov_base = str->buf;
	vec->iov_len = str->length;
}

APT_END_EXTERN_C

#endif /* APT_STRING_H */
