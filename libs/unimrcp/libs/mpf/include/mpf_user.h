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

#ifndef __MPF_USER_H__
#define __MPF_USER_H__

/**
 * @file mpf_user.h
 * @brief MPF User Interface
 */ 

#include "mpf_types.h"

APT_BEGIN_EXTERN_C

/**
 * Create MPF context.
 * @param obj the external object associated with context
 * @param max_termination_count the max number of terminations in context
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_context_t*) mpf_context_create(void *obj, apr_size_t max_termination_count, apr_pool_t *pool);

/**
 * Destroy MPF context.
 * @param context the context to destroy
 */
MPF_DECLARE(apt_bool_t) mpf_context_destroy(mpf_context_t *context);

/**
 * Get external object associated with MPF context.
 * @param context the context to get object from
 */
MPF_DECLARE(void*) mpf_context_object_get(mpf_context_t *context);


APT_END_EXTERN_C

#endif /*__MPF_USER_H__*/
