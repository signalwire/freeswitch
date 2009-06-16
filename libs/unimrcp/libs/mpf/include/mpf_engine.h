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

#ifndef __MPF_ENGINE_H__
#define __MPF_ENGINE_H__

/**
 * @file mpf_engine.h
 * @brief Media Processing Framework Engine
 */ 

#include "apt_task.h"
#include "mpf_message.h"

APT_BEGIN_EXTERN_C

/**
 * Create MPF engine.
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_engine_t*) mpf_engine_create(apr_pool_t *pool);

/**
 * Create MPF codec manager.
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_codec_manager_t*) mpf_engine_codec_manager_create(apr_pool_t *pool);

/**
 * Register MPF codec manager.
 * @param engine the engine to register codec manager for
 * @param codec_manager the codec manager to register
 */
MPF_DECLARE(apt_bool_t) mpf_engine_codec_manager_register(mpf_engine_t *engine, const mpf_codec_manager_t *codec_manager);

/**
 * Get task.
 * @param engine the engine to get task from
 */
MPF_DECLARE(apt_task_t*) mpf_task_get(mpf_engine_t *engine);

/**
 * Set task msg type to send responses and events with.
 * @param engine the engine to set task msg type for
 * @param type the type to set
 */
MPF_DECLARE(void) mpf_engine_task_msg_type_set(mpf_engine_t *engine, apt_task_msg_type_e type);


APT_END_EXTERN_C

#endif /*__MPF_ENGINE_H__*/
