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

#include <nks/errno.h>

#include "fspr.h"
#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_strings.h"
#include "fspr_arch_thread_mutex.h"
#include "fspr_arch_thread_cond.h"
#include "fspr_portable.h"

static fspr_status_t thread_cond_cleanup(void *data)
{
    fspr_thread_cond_t *cond = (fspr_thread_cond_t *)data;

    NXCondFree(cond->cond);        
    return APR_SUCCESS;
} 

APR_DECLARE(fspr_status_t) fspr_thread_cond_create(fspr_thread_cond_t **cond,
                                                 fspr_pool_t *pool)
{
    fspr_thread_cond_t *new_cond = NULL;

    new_cond = (fspr_thread_cond_t *)fspr_pcalloc(pool, sizeof(fspr_thread_cond_t));
	
	if(new_cond ==NULL) {
        return APR_ENOMEM;
    }     
    new_cond->pool = pool;

    new_cond->cond = NXCondAlloc(NULL);
    
    if(new_cond->cond == NULL)
        return APR_ENOMEM;

    fspr_pool_cleanup_register(new_cond->pool, new_cond, 
                                (void*)thread_cond_cleanup,
                                fspr_pool_cleanup_null);
   *cond = new_cond;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_wait(fspr_thread_cond_t *cond,
                                               fspr_thread_mutex_t *mutex)
{
    if (NXCondWait(cond->cond, mutex->mutex) != 0)
        return APR_EINTR;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_timedwait(fspr_thread_cond_t *cond,
                                                    fspr_thread_mutex_t *mutex,
                                                    fspr_interval_time_t timeout){
    if (NXCondTimedWait(cond->cond, mutex->mutex, 
        (timeout*1000)/NXGetSystemTick()) == NX_ETIMEDOUT) {
        return APR_TIMEUP;
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_signal(fspr_thread_cond_t *cond)
{
    NXCondSignal(cond->cond);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_broadcast(fspr_thread_cond_t *cond)
{
    NXCondBroadcast(cond->cond);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_destroy(fspr_thread_cond_t *cond)
{
    fspr_status_t stat;
    if ((stat = thread_cond_cleanup(cond)) == APR_SUCCESS) {
        fspr_pool_cleanup_kill(cond->pool, cond, thread_cond_cleanup);
        return APR_SUCCESS;
    }
    return stat;
}

APR_POOL_IMPLEMENT_ACCESSOR(thread_cond)

