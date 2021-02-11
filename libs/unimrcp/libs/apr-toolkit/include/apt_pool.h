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

#ifndef APT_POOL_H
#define APT_POOL_H

/**
 * @file apt_pool.h
 * @brief APR pool management
 */ 

/**
 * Wrappers around APR pool creation 
 * allow to control memory allocation policy project uses
 */

#include "apt.h"

APT_BEGIN_EXTERN_C

/**
 * Create APR pool
 */
APT_DECLARE(apr_pool_t*) apt_pool_create(void);

/**
 * Create APR subpool pool
 * @param parent the parent pool
 */
APT_DECLARE(apr_pool_t*) apt_subpool_create(apr_pool_t *parent);

APT_END_EXTERN_C

#endif /* APT_POOL_H */
