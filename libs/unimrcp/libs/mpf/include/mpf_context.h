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

#ifndef __MPF_CONTEXT_H__
#define __MPF_CONTEXT_H__

/**
 * @file mpf_context.h
 * @brief MPF Context
 */ 

#include "mpf_object.h"
#include "apt_obj_list.h"

APT_BEGIN_EXTERN_C

/** Definition of table item used in context */
typedef void* table_item_t;

/** Media processing context */
struct mpf_context_t {
    /** Pool to allocate memory from */
	apr_pool_t        *pool;
    /** External object */
	void              *obj;
    /** Set when context is addded to the list to ensure quick find on delete */
	apt_list_elem_t   *elem;

	/** Max number of terminations */
	apr_size_t          max_termination_count;
	/** Current number of terminations */
	apr_size_t          termination_count;
	/** Table, which holds terminations and topology */
	table_item_t      **table;
};


/**
 * Add termination to context.
 * @param context the context to add termination to
 * @param termination the termination to add
 */
MPF_DECLARE(apt_bool_t) mpf_context_termination_add(mpf_context_t *context, mpf_termination_t *termination);

/**
 * Subtract termination from context.
 * @param context the context to subtract termination from
 * @param termination the termination to subtract
 */
MPF_DECLARE(apt_bool_t) mpf_context_termination_subtract(mpf_context_t *context, mpf_termination_t *termination);

/**
 * Apply topology.
 * @param context the context which holds the termination
 * @param termination the termination to apply toplogy for
 */
MPF_DECLARE(apt_bool_t) mpf_context_topology_apply(mpf_context_t *context, mpf_termination_t *termination);

/**
 * Destroy topology.
 * @param context the context which holds the termination
 * @param termination the termination to destroy toplogy for
 */
MPF_DECLARE(apt_bool_t) mpf_context_topology_destroy(mpf_context_t *context, mpf_termination_t *termination);

/**
 * Process context.
 * @param context the context
 */
MPF_DECLARE(apt_bool_t) mpf_context_process(mpf_context_t *context);


APT_END_EXTERN_C

#endif /*__MPF_CONTEXT_H__*/
