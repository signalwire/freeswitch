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
 * $Id: apt_obj_list.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef APT_OBJ_LIST_H
#define APT_OBJ_LIST_H

/**
 * @file apt_obj_list.h
 * @brief List of Opaque void* Objects
 */ 

#include "apt.h"

APT_BEGIN_EXTERN_C


/** Opaque list declaration */
typedef struct apt_obj_list_t apt_obj_list_t;
/** Opaque list element declaration */
typedef struct apt_list_elem_t apt_list_elem_t;

/**
 * Create list.
 * @param pool the pool to allocate list from
 * @return the created list
 */
APT_DECLARE(apt_obj_list_t*) apt_list_create(apr_pool_t *pool);

/**
 * Destroy list.
 * @param list the list to destroy
 */
APT_DECLARE(void) apt_list_destroy(apt_obj_list_t *list);

/**
 * Push object to the list as first in, first out.
 * @param list the list to push object to
 * @param obj the object to push
 * @param pool the pool to allocate list element from
 * @return the inserted element
 */
APT_DECLARE(apt_list_elem_t*) apt_list_push_back(apt_obj_list_t *list, void *obj, apr_pool_t *pool);

/**
 * Pop object from the list as first in, first out.
 * @param list the list to pop message from
 * @return the popped object (if any)
 */
APT_DECLARE(void*) apt_list_pop_front(apt_obj_list_t *list);

/**
 * Retrieve object of the first element in the list.
 * @param list the list to retrieve from
 */
APT_DECLARE(void*) apt_list_head(const apt_obj_list_t *list);

/**
 * Retrieve object of the last element in the list.
 * @param list the list to retrieve from
 */
APT_DECLARE(void*) apt_obj_list_tail(const apt_obj_list_t *list);


/**
 * Retrieve the first element of the list.
 * @param list the list to retrieve from
 */
APT_DECLARE(apt_list_elem_t*) apt_list_first_elem_get(const apt_obj_list_t *list);

/**
 * Retrieve the last element of the list.
 * @param list the list to retrieve from
 */
APT_DECLARE(apt_list_elem_t*) apt_list_last_elem_get(const apt_obj_list_t *list);

/**
 * Retrieve the next element of the list.
 * @param list the list to retrieve from
 * @param elem the element to retrieve next element from
 */
APT_DECLARE(apt_list_elem_t*) apt_list_next_elem_get(const apt_obj_list_t *list, apt_list_elem_t *elem);

/**
 * Retrieve the prev element of the list.
 * @param list the list to retrieve from
 * @param elem the element to retrieve prev element from
 */
APT_DECLARE(apt_list_elem_t*) apt_list_prev_elem_get(const apt_obj_list_t *list, apt_list_elem_t *elem);

/**
 * Insert element to the list.
 * @param list the list to insert element to
 * @param elem the element to insert before
 * @param obj the object to insert
 * @param pool the pool to allocate list element from
 * @return the inserted element
 */
APT_DECLARE(apt_list_elem_t*) apt_list_elem_insert(apt_obj_list_t *list, apt_list_elem_t *elem, void *obj, apr_pool_t *pool);

/**
 * Remove element from the list.
 * @param list the list to remove element from
 * @param elem the element to remove
 * @return the next element (if any)
 */
APT_DECLARE(apt_list_elem_t*) apt_list_elem_remove(apt_obj_list_t *list, apt_list_elem_t *elem);

/**
 * Query whether the list is empty.
 * @param list the list to query
 * @return TRUE if empty, otherwise FALSE
 */
APT_DECLARE(apt_bool_t) apt_list_is_empty(const apt_obj_list_t *list);

/**
 * Retrieve the object associated with element.
 * @param elem the element to retrieve object from
 */
APT_DECLARE(void*) apt_list_elem_object_get(const apt_list_elem_t *elem);


APT_END_EXTERN_C

#endif /* APT_OBJ_LIST_H */
