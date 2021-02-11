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

#ifndef APT_STRING_TABLE_H
#define APT_STRING_TABLE_H

/**
 * @file apt_string_table.h
 * @brief Generic String Table
 */ 

#include "apt_string.h"

APT_BEGIN_EXTERN_C


/** String table item declaration */
typedef struct apt_str_table_item_t apt_str_table_item_t;

/** String table item definition */
struct apt_str_table_item_t {
	/** String value associated with id */
	apt_str_t  value;
	/** Index of the unique (key) character to compare */
	apr_size_t key;
};


/**
 * Get the string by a given id.
 * @param table the table to get string from
 * @param size the size of the table
 * @param id the id to get string by
 * @return the string associated with the id, or NULL if the id is invalid
 */
APT_DECLARE(const apt_str_t*) apt_string_table_str_get(const apt_str_table_item_t table[], apr_size_t size, apr_size_t id);

/**
 * Find the id associated with a given string.
 * @param table the table to search for the id
 * @param size the size of the table
 * @param value the string to search for
 * @return the id associated with the string, or invalid id if string cannot be matched
 */
APT_DECLARE(apr_size_t) apt_string_table_id_find(const apt_str_table_item_t table[], apr_size_t size, const apt_str_t *value);


APT_END_EXTERN_C

#endif /* APT_STRING_TABLE_H */
