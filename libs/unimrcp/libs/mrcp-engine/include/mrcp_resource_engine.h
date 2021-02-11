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

#ifndef MRCP_RESOURCE_ENGINE_H
#define MRCP_RESOURCE_ENGINE_H

/**
 * @file mrcp_resource_engine.h
 * @brief Legacy MRCP Resource Engine
 * @deprecated @see mrcp_engine_plugin.h and mrcp_engine_impl.h
 */ 

#include "mrcp_engine_plugin.h"
#include "mrcp_engine_impl.h"

APT_BEGIN_EXTERN_C

/** Termorary define legacy mrcp_resource_engine_t as mrcp_engine_t */
typedef mrcp_engine_t mrcp_resource_engine_t;

/** 
 * Create resource engine
 * @deprecated @see mrcp_engine_create
 */
static APR_INLINE mrcp_engine_t* mrcp_resource_engine_create(
					mrcp_resource_id resource_id,
					void *obj, 
					const mrcp_engine_method_vtable_t *vtable,
					apr_pool_t *pool)
{
	return mrcp_engine_create(resource_id,obj,vtable,pool);
}

APT_END_EXTERN_C

#endif /* MRCP_RESOURCE_ENGINE_H */
