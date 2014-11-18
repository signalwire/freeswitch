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
 * $Id: apt_pool.c 2136 2014-07-04 06:33:36Z achaloyan@gmail.com $
 */

#include "apt_pool.h"
#include "apt_log.h"

#define OWN_ALLOCATOR_PER_POOL

static int apt_abort_fn(int retcode)
{
	apt_log(APT_LOG_MARK,APT_PRIO_CRITICAL,"APR Abort Called [%d]", retcode);
	return 0;
}

APT_DECLARE(apr_pool_t*) apt_pool_create()
{
	apr_pool_t *pool = NULL;

#ifdef OWN_ALLOCATOR_PER_POOL
	apr_allocator_t *allocator = NULL;
	apr_thread_mutex_t *mutex = NULL;

	if(apr_allocator_create(&allocator) == APR_SUCCESS) {
		if(apr_pool_create_ex(&pool,NULL,apt_abort_fn,allocator) == APR_SUCCESS) {
			apr_allocator_owner_set(allocator,pool);
			apr_thread_mutex_create(&mutex,APR_THREAD_MUTEX_NESTED,pool);
			apr_allocator_mutex_set(allocator,mutex);
			apr_pool_mutex_set(pool,mutex); 
		}
	}
#else
	apr_pool_create(&pool,NULL);
#endif
	return pool;
}

APT_DECLARE(apr_pool_t*) apt_subpool_create(apr_pool_t *parent)
{
	apr_pool_t *pool = NULL;
	apr_pool_create(&pool,parent);
	return pool;
}
