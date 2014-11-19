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
 * $Id: apt_string_table.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include <ctype.h>
#include "apt_string_table.h"

/* Get the string by a given id. */
APT_DECLARE(const apt_str_t*) apt_string_table_str_get(const apt_str_table_item_t table[], apr_size_t size, apr_size_t id)
{
	if(id < size) {
		return &table[id].value;
	}
	return NULL;
}

/* Find the id associated with a given string from the table */
APT_DECLARE(apr_size_t) apt_string_table_id_find(const apt_str_table_item_t table[], apr_size_t size, const apt_str_t *value)
{
	/* Key character is stored within each apt_string_table_item.
	At first, key characters must be matched in a loop crossing the items.
	Then whole strings should be compared only for the matched item.
	Key characters should be automatically generated once for a given string table. */

	apr_size_t i;
	const apt_str_table_item_t *item;
	for(i=0; i<size; i++) {
		item = &table[i];
		if(item->value.length != value->length) {
			/* lengths of th strings differ, just contninue */
			continue;
		}
		/* check whether key is available */
		if(item->key < value->length) {
			/* check whether values are matched by key (using no case compare) */
			if(value->length == item->value.length && 
				tolower(item->value.buf[item->key]) == tolower(value->buf[item->key])) {
				/* whole strings must be compared to ensure, should be done only once for each lookup */
				if(apt_string_compare(&item->value,value) == TRUE) {
					return i;
				}
			}
		}
		else {
			/* no key available, just compare whole strings */
			if(apt_string_compare(&item->value,value) == TRUE) {
				return i;
			}
		}
	}

	/* no match found, return invalid id */
	return size;
}
