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

#ifdef WIN32
#pragma warning(disable: 4127)
#endif
#include <apr_ring.h>
#include "apt_obj_list.h"

struct apt_list_elem_t {
	APR_RING_ENTRY(apt_list_elem_t) link;
	void                           *obj;
};

struct apt_obj_list_t {
	APR_RING_HEAD(apt_list_head_t, apt_list_elem_t) head;
	apr_pool_t                                     *pool;
};



APT_DECLARE(apt_obj_list_t*) apt_list_create(apr_pool_t *pool)
{
	apt_obj_list_t *list = apr_palloc(pool, sizeof(apt_obj_list_t));
	list->pool = pool;
	APR_RING_INIT(&list->head, apt_list_elem_t, link);
	return list;
}

APT_DECLARE(void) apt_list_destroy(apt_obj_list_t *list)
{
	/* nothing to do, the list is allocated from the pool */
}

APT_DECLARE(apt_list_elem_t*) apt_list_push_back(apt_obj_list_t *list, void *obj, apr_pool_t *pool)
{
	apt_list_elem_t *elem = apr_palloc(pool,sizeof(apt_list_elem_t));
	elem->obj = obj;

	APR_RING_INSERT_TAIL(&list->head,elem,apt_list_elem_t,link);
	return elem;
}

APT_DECLARE(void*) apt_list_pop_front(apt_obj_list_t *list)
{
	apt_list_elem_t *elem;
	if(APR_RING_EMPTY(&list->head,apt_list_elem_t,link)) {
		return NULL;
	}
	elem = APR_RING_FIRST(&list->head);
	APR_RING_REMOVE(elem,link);
	return elem->obj;
}

APT_DECLARE(void*) apt_list_head(apt_obj_list_t *list)
{
	apt_list_elem_t *elem;
	if(APR_RING_EMPTY(&list->head,apt_list_elem_t,link)) {
		return NULL;
	}
	elem = APR_RING_FIRST(&list->head);
	return elem->obj;
}

APT_DECLARE(void*) apt_obj_list_tail(apt_obj_list_t *list)
{
	apt_list_elem_t *elem;
	if(APR_RING_EMPTY(&list->head,apt_list_elem_t,link)) {
		return NULL;
	}
	elem = APR_RING_LAST(&list->head);
	return elem->obj;
}

APT_DECLARE(apt_list_elem_t*) apt_list_first_elem_get(apt_obj_list_t *list)
{
	if(APR_RING_EMPTY(&list->head,apt_list_elem_t,link)) {
		return NULL;
	}
	return APR_RING_FIRST(&list->head);
}

APT_DECLARE(apt_list_elem_t*) apt_list_last_elem_get(apt_obj_list_t *list)
{
	if(APR_RING_EMPTY(&list->head,apt_list_elem_t,link)) {
		return NULL;
	}
	return APR_RING_LAST(&list->head);
}

APT_DECLARE(apt_list_elem_t*) apt_list_next_elem_get(apt_obj_list_t *list, apt_list_elem_t *elem)
{
	apt_list_elem_t *next_elem = APR_RING_NEXT(elem,link);
	if(next_elem == APR_RING_SENTINEL(&list->head,apt_list_elem_t,link)) {
		next_elem = NULL;
	}
	return next_elem;
}

APT_DECLARE(apt_list_elem_t*) apt_list_prev_elem_get(apt_obj_list_t *list, apt_list_elem_t *elem)
{
	apt_list_elem_t *prev_elem = APR_RING_PREV(elem,link);
	if(prev_elem == APR_RING_SENTINEL(&list->head,apt_list_elem_t,link)) {
		prev_elem = NULL;
	}
	return prev_elem;
}

APT_DECLARE(apt_list_elem_t*) apt_list_elem_insert(apt_obj_list_t *list, apt_list_elem_t *elem, void *obj, apr_pool_t *pool)
{
	apt_list_elem_t *new_elem = apr_palloc(pool,sizeof(apt_list_elem_t));
	new_elem->obj = obj;
	APR_RING_INSERT_BEFORE(elem,new_elem,link);
	return new_elem;
}

APT_DECLARE(apt_list_elem_t*) apt_list_elem_remove(apt_obj_list_t *list, apt_list_elem_t *elem)
{
	apt_list_elem_t *next_elem = APR_RING_NEXT(elem,link);
	APR_RING_REMOVE(elem,link);
	if(next_elem == APR_RING_SENTINEL(&list->head,apt_list_elem_t,link)) {
		next_elem = NULL;
	}
	return next_elem;
}

APT_DECLARE(apt_bool_t) apt_list_is_empty(apt_obj_list_t *list)
{
	if(APR_RING_EMPTY(&list->head,apt_list_elem_t,link)) {
		return TRUE;
	}
	return FALSE;
}

APT_DECLARE(void*) apt_list_elem_object_get(apt_list_elem_t *elem)
{
	return elem->obj;
}
