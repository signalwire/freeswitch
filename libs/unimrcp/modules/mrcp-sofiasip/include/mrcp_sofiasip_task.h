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

#ifndef MRCP_SOFIASIP_TASK_H
#define MRCP_SOFIASIP_TASK_H

/**
 * @file mrcp_sofiasip_task.h
 * @brief Sofia-SIP Task
 */ 

#include "apt_task.h"

APT_BEGIN_EXTERN_C

/** Opaque Sofia-SIP task declaration */
typedef struct mrcp_sofia_task_t mrcp_sofia_task_t;

/** Function prototype to create a nua instance */
typedef nua_t* (*create_nua_f)(void *obj, su_root_t *root);

/**
 * Create Sofia-SIP task.
 * @param nua_creator the nua creator method
 * @param obj the external object to pass to nua creator method
 * @param msg_pool the pool of task messages
 * @param pool the pool to allocate memory from
 */
APT_DECLARE(mrcp_sofia_task_t*) mrcp_sofia_task_create(
										create_nua_f nua_creator,
										void *obj,
										apt_task_msg_pool_t *msg_pool,
										apr_pool_t *pool);

/**
 * Destroy Sofia-SIP task.
 * @param task the task to destroy
 */
APT_DECLARE(apt_bool_t) mrcp_sofia_task_destroy(mrcp_sofia_task_t *task);

/**
 * Start Sofia-SIP task.
 * @param task the task to start
 */
APT_DECLARE(apt_bool_t) mrcp_sofia_task_start(mrcp_sofia_task_t *task);

/**
 * Terminate Sofia-SIP task.
 * @param task the task to terminate
 */
APT_DECLARE(apt_bool_t) mrcp_sofia_task_terminate(mrcp_sofia_task_t *task);

/**
 * Break main loop of Sofia-SIP task.
 * @param task the task to break
 */
APT_DECLARE(void) mrcp_sofia_task_break(mrcp_sofia_task_t *task);

/**
 * Get task base.
 * @param task the Sofia-SIP task to get task base from
 */
APT_DECLARE(apt_task_t*) mrcp_sofia_task_base_get(const mrcp_sofia_task_t *task);

/**
 * Get task vtable.
 * @param task the Sofia-SIP task to get vtable from
 */
APT_DECLARE(apt_task_vtable_t*) mrcp_sofia_task_vtable_get(const mrcp_sofia_task_t *task);

/**
 * Get external object.
 * @param task the Sofia-SIP task to get object from
 */
APT_DECLARE(void*) mrcp_sofia_task_object_get(const mrcp_sofia_task_t *task);

/**
 * Get su_root object.
 * @param task the Sofia-SIP task to get su_root object from
 */
APT_DECLARE(su_root_t*) mrcp_sofia_task_su_root_get(const mrcp_sofia_task_t *task);

/**
 * Get nua object.
 * @param task the Sofia-SIP task to get nua object from
 */
APT_DECLARE(nua_t*) mrcp_sofia_task_nua_get(const mrcp_sofia_task_t *task);


APT_END_EXTERN_C

#endif /* MRCP_SOFIASIP_TASK_H */
