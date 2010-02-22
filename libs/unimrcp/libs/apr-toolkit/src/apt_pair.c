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

#include "apt_pair.h"

/** Create array of name-value pairs */
APT_DECLARE(apt_pair_arr_t*) apt_pair_array_create(apr_size_t initial_size, apr_pool_t *pool)
{
	return apr_array_make(pool,(int)initial_size,sizeof(apt_pair_t));
}

/** Copy array of name-value pairs */
APT_DECLARE(apt_pair_arr_t*) apt_pair_array_copy(const apt_pair_arr_t *src_arr, apr_pool_t *pool)
{
	int i;
	const apt_pair_t *src_pair;
	apt_pair_t *pair;
	apt_pair_arr_t *arr;
	if(!src_arr) {
		return NULL;
	}
	arr = apr_array_copy(pool,src_arr);
	for(i=0; i<arr->nelts; i++) {
		pair = &APR_ARRAY_IDX(arr,i,apt_pair_t);
		src_pair = &APR_ARRAY_IDX(src_arr,i,const apt_pair_t);
		apt_pair_copy(pair,src_pair,pool);
	}
	return arr;
}


/** Append name-value pair */
APT_DECLARE(apt_bool_t) apt_pair_array_append(apt_pair_arr_t *arr, const apt_str_t *name, const apt_str_t *value, apr_pool_t *pool)
{
	apt_pair_t *pair = apr_array_push(arr);
	apt_pair_init(pair);
	if(name) {
		apt_string_copy(&pair->name,name,pool);
	}
	if(value) {
		apt_string_copy(&pair->value,value,pool);
	}
	return TRUE;
}

/** Find name-value pair by name */
APT_DECLARE(const apt_pair_t*) apt_pair_array_find(const apt_pair_arr_t *arr, const apt_str_t *name)
{
	int i;
	apt_pair_t *pair;
	for(i=0; i<arr->nelts; i++) {
		pair = &APR_ARRAY_IDX(arr,i,apt_pair_t);
		if(apt_string_compare(&pair->name,name) == TRUE) {
			return pair;
		}
	}
	return NULL;
}

/** Get size of pair array */
APT_DECLARE(int) apt_pair_array_size_get(const apt_pair_arr_t *arr)
{
	return arr->nelts;
}

/** Get name-value pair by id */
APT_DECLARE(const apt_pair_t*) apt_pair_array_get(const apt_pair_arr_t *arr, int id)
{
	if(id < arr->nelts) {
		return &APR_ARRAY_IDX(arr,id,apt_pair_t);
	}
	return NULL;
}
