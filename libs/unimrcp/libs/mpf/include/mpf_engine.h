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
 * $Id: mpf_engine.h 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#ifndef MPF_ENGINE_H
#define MPF_ENGINE_H

/**
 * @file mpf_engine.h
 * @brief Media Processing Framework Engine
 */ 

#include "apt_task.h"
#include "mpf_message.h"

APT_BEGIN_EXTERN_C

/** MPF task message definition */
typedef apt_task_msg_t mpf_task_msg_t;

/**
 * Create MPF engine.
 * @param id the identifier of the engine
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_engine_t*) mpf_engine_create(const char *id, apr_pool_t *pool);

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
 * Create MPF context.
 * @param engine the engine to create context for
 * @param name the informative name of the context
 * @param obj the external object associated with context
 * @param max_termination_count the max number of terminations in context
 * @param pool the pool to allocate memory from
 */
MPF_DECLARE(mpf_context_t*) mpf_engine_context_create(
								mpf_engine_t *engine,
								const char *name,
								void *obj, 
								apr_size_t max_termination_count, 
								apr_pool_t *pool);

/**
 * Destroy MPF context.
 * @param context the context to destroy
 */
MPF_DECLARE(apt_bool_t) mpf_engine_context_destroy(mpf_context_t *context);

/**
 * Get external object associated with MPF context.
 * @param context the context to get object from
 */
MPF_DECLARE(void*) mpf_engine_context_object_get(const mpf_context_t *context);

/**
 * Get task.
 * @param engine the engine to get task from
 */
MPF_DECLARE(apt_task_t*) mpf_task_get(const mpf_engine_t *engine);

/**
 * Set task msg type to send responses and events with.
 * @param engine the engine to set task msg type for
 * @param type the type to set
 */
MPF_DECLARE(void) mpf_engine_task_msg_type_set(mpf_engine_t *engine, apt_task_msg_type_e type);

/**
 * Create task message(if not created) and add MPF termination message to it.
 * @param engine the engine task message belongs to
 * @param command_id the MPF command identifier
 * @param context the context to add termination to
 * @param termination the termination to add
 * @param descriptor the termination dependent descriptor
 * @param task_msg the task message to create and add constructed MPF message to
 */
MPF_DECLARE(apt_bool_t) mpf_engine_termination_message_add(
							mpf_engine_t *engine,
							mpf_command_type_e command_id,
							mpf_context_t *context,
							mpf_termination_t *termination,
							void *descriptor,
							mpf_task_msg_t **task_msg);

/**
 * Create task message(if not created) and add MPF association message to it.
 * @param engine the engine task message belongs to
 * @param command_id the MPF command identifier
 * @param context the context to add association of terminations for
 * @param termination the termination to associate
 * @param assoc_termination the termination to associate
 * @param task_msg the task message to create and add constructed MPF message to
 */
MPF_DECLARE(apt_bool_t) mpf_engine_assoc_message_add(
							mpf_engine_t *engine,
							mpf_command_type_e command_id,
							mpf_context_t *context,
							mpf_termination_t *termination,
							mpf_termination_t *assoc_termination,
							mpf_task_msg_t **task_msg);

/**
 * Create task message(if not created) and add MPF topology message to it.
 * @param engine the engine task message belongs to
 * @param command_id the MPF command identifier
 * @param context the context to modify topology for
 * @param task_msg the task message to create and add constructed MPF message to
 */
MPF_DECLARE(apt_bool_t) mpf_engine_topology_message_add(
							mpf_engine_t *engine,
							mpf_command_type_e command_id,
							mpf_context_t *context,
							mpf_task_msg_t **task_msg);

/**
 * Send MPF task message.
 * @param engine the engine to send task message to
 * @param task_msg the task message to send
 */
MPF_DECLARE(apt_bool_t) mpf_engine_message_send(mpf_engine_t *engine, mpf_task_msg_t **task_msg);

/**
 * Set scheduler rate.
 * @param engine the engine to set rate for
 * @param rate the rate (n times faster than real-time)
 */
MPF_DECLARE(apt_bool_t) mpf_engine_scheduler_rate_set(mpf_engine_t *engine, unsigned long rate);

/**
 * Get the identifier of the engine .
 * @param engine the engine to get name of
 */
MPF_DECLARE(const char*) mpf_engine_id_get(const mpf_engine_t *engine);


APT_END_EXTERN_C

#endif /* MPF_ENGINE_H */
