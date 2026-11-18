/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef APR_ENV_H
#define APR_ENV_H
/**
 * @file fspr_env.h
 * @brief APR Environment functions
 */
#include "fspr_errno.h"
#include "fspr_pools.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/**
 * @defgroup fspr_env Functions for manupulating the environment
 * @ingroup APR 
 * @{
 */

/**
 * Get the value of an environment variable
 * @param value the returned value, allocated from @a pool
 * @param envvar the name of the environment variable
 * @param pool where to allocate @a value and any temporary storage from
 */
APR_DECLARE(fspr_status_t) fspr_env_get(char **value, const char *envvar,
                                      fspr_pool_t *pool);

/**
 * Set the value of an environment variable
 * @param envvar the name of the environment variable
 * @param value the value to set
 * @param pool where to allocate temporary storage from
 */
APR_DECLARE(fspr_status_t) fspr_env_set(const char *envvar, const char *value,
                                      fspr_pool_t *pool);

/**
 * Delete a variable from the environment
 * @param envvar the name of the environment variable
 * @param pool where to allocate temporary storage from
 */
APR_DECLARE(fspr_status_t) fspr_env_delete(const char *envvar, fspr_pool_t *pool);

/** @} */

#ifdef __cplusplus
}
#endif

#endif  /* ! APR_ENV_H */
