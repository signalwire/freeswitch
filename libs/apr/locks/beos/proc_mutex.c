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

/*Read/Write locking implementation based on the MultiLock code from
 * Stephen Beaulieu <hippo@be.com>
 */
 
#include "fspr_arch_proc_mutex.h"
#include "fspr_strings.h"
#include "fspr_portable.h"

static fspr_status_t _proc_mutex_cleanup(void * data)
{
    fspr_proc_mutex_t *lock = (fspr_proc_mutex_t*)data;
    if (lock->LockCount != 0) {
        /* we're still locked... */
    	while (atomic_add(&lock->LockCount , -1) > 1){
    	    /* OK we had more than one person waiting on the lock so 
    	     * the sem is also locked. Release it until we have no more
    	     * locks left.
    	     */
            release_sem (lock->Lock);
    	}
    }
    delete_sem(lock->Lock);
    return APR_SUCCESS;
}    

APR_DECLARE(fspr_status_t) fspr_proc_mutex_create(fspr_proc_mutex_t **mutex,
                                                const char *fname,
                                                fspr_lockmech_e mech,
                                                fspr_pool_t *pool)
{
    fspr_proc_mutex_t *new;
    fspr_status_t stat = APR_SUCCESS;
  
    if (mech != APR_LOCK_DEFAULT) {
        return APR_ENOTIMPL;
    }

    new = (fspr_proc_mutex_t *)fspr_pcalloc(pool, sizeof(fspr_proc_mutex_t));
    if (new == NULL){
        return APR_ENOMEM;
    }
    
    if ((stat = create_sem(0, "APR_Lock")) < B_NO_ERROR) {
        _proc_mutex_cleanup(new);
        return stat;
    }
    new->LockCount = 0;
    new->Lock = stat;  
    new->pool  = pool;

    fspr_pool_cleanup_register(new->pool, (void *)new, _proc_mutex_cleanup,
                              fspr_pool_cleanup_null);

    (*mutex) = new;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_child_init(fspr_proc_mutex_t **mutex,
                                                    const char *fname,
                                                    fspr_pool_t *pool)
{
    return APR_SUCCESS;
}
    
APR_DECLARE(fspr_status_t) fspr_proc_mutex_lock(fspr_proc_mutex_t *mutex)
{
    int32 stat;
    
	if (atomic_add(&mutex->LockCount, 1) > 0) {
		if ((stat = acquire_sem(mutex->Lock)) < B_NO_ERROR) {
		    atomic_add(&mutex->LockCount, -1);
		    return stat;
		}
	}
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_trylock(fspr_proc_mutex_t *mutex)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_unlock(fspr_proc_mutex_t *mutex)
{
    int32 stat;
    
	if (atomic_add(&mutex->LockCount, -1) > 1) {
        if ((stat = release_sem(mutex->Lock)) < B_NO_ERROR) {
            atomic_add(&mutex->LockCount, 1);
            return stat;
        }
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_destroy(fspr_proc_mutex_t *mutex)
{
    fspr_status_t stat;
    if ((stat = _proc_mutex_cleanup(mutex)) == APR_SUCCESS) {
        fspr_pool_cleanup_kill(mutex->pool, mutex, _proc_mutex_cleanup);
        return APR_SUCCESS;
    }
    return stat;
}

APR_DECLARE(fspr_status_t) fspr_proc_mutex_cleanup(void *mutex)
{
    return _proc_mutex_cleanup(mutex);
}


APR_DECLARE(const char *) fspr_proc_mutex_lockfile(fspr_proc_mutex_t *mutex)
{
    return NULL;
}

APR_DECLARE(const char *) fspr_proc_mutex_name(fspr_proc_mutex_t *mutex)
{
    return "beossem";
}

APR_DECLARE(const char *) fspr_proc_mutex_defname(void)
{
    return "beossem";
}

APR_POOL_IMPLEMENT_ACCESSOR(proc_mutex)

/* Implement OS-specific accessors defined in fspr_portable.h */

APR_DECLARE(fspr_status_t) fspr_os_proc_mutex_get(fspr_os_proc_mutex_t *ospmutex,
                                                fspr_proc_mutex_t *pmutex)
{
    ospmutex->sem = pmutex->Lock;
    ospmutex->ben = pmutex->LockCount;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_proc_mutex_put(fspr_proc_mutex_t **pmutex,
                                                fspr_os_proc_mutex_t *ospmutex,
                                                fspr_pool_t *pool)
{
    if (pool == NULL) {
        return APR_ENOPOOL;
    }
    if ((*pmutex) == NULL) {
        (*pmutex) = (fspr_proc_mutex_t *)fspr_pcalloc(pool, sizeof(fspr_proc_mutex_t));
        (*pmutex)->pool = pool;
    }
    (*pmutex)->Lock = ospmutex->sem;
    (*pmutex)->LockCount = ospmutex->ben;
    return APR_SUCCESS;
}

