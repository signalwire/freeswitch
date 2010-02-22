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

#ifndef __APT_PAIR_H__
#define __APT_PAIR_H__

/**
 * @file apt_pair.h
 * @brief Generic Name-Value Pair
 */ 

#include <apr_tables.h>
#include "apt_string.h"

APT_BEGIN_EXTERN_C

/** Name-value declaration */
typedef struct apt_pair_t apt_pair_t;

/** Generic name-value pair definition ("name:value") */
struct apt_pair_t {
	/** The name */
	apt_str_t name;
	/** The value */
	apt_str_t value;
};

/** Dynamic array of name-value pairs */
typedef apr_array_header_t apt_pair_arr_t;

/** Initialize name-value pair */
static APR_INLINE void apt_pair_init(apt_pair_t *pair)
{
	apt_string_reset(&pair->name);
	apt_string_reset(&pair->value);
}

/** Copy name-value pair */
static APR_INLINE void apt_pair_copy(apt_pair_t *pair, const apt_pair_t *src_pair, apr_pool_t *pool)
{
	apt_string_copy(&pair->name,&src_pair->name,pool);
	apt_string_copy(&pair->value,&src_pair->value,pool);
}

/** Create array of name-value pairs */
APT_DECLARE(apt_pair_arr_t*) apt_pair_array_create(apr_size_t initial_size, apr_pool_t *pool);
/** Copy array of name-value pairs */
APT_DECLARE(apt_pair_arr_t*) apt_pair_array_copy(const apt_pair_arr_t *src, apr_pool_t *pool);
/** Append name-value pair */
APT_DECLARE(apt_bool_t) apt_pair_array_append(apt_pair_arr_t *arr, const apt_str_t *name, const apt_str_t *value, apr_pool_t *pool);
/** Find name-value pair by name */
APT_DECLARE(const apt_pair_t*) apt_pair_array_find(const apt_pair_arr_t *arr, const apt_str_t *name);
/** Get size of pair array */
APT_DECLARE(int) apt_pair_array_size_get(const apt_pair_arr_t *arr);
/** Get name-value pair by id */
APT_DECLARE(const apt_pair_t*) apt_pair_array_get(const apt_pair_arr_t *arr, int id);

APT_END_EXTERN_C

#endif /*__APT_PAIR_H__*/
