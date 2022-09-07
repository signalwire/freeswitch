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

#include "fspr_arch_threadproc.h"
#include "fspr_portable.h"

APR_DECLARE(fspr_status_t) fspr_threadattr_create(fspr_threadattr_t **new, fspr_pool_t *pool)
{
    (*new) = (fspr_threadattr_t *)fspr_palloc(pool, 
              sizeof(fspr_threadattr_t));

    if ((*new) == NULL) {
        return APR_ENOMEM;
    }

    (*new)->pool = pool;
	(*new)->attr = (int32)B_NORMAL_PRIORITY;

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_detach_set(fspr_threadattr_t *attr, fspr_int32_t on)
{
	if (on == 1){
		attr->detached = 1;
	} else {
		attr->detached = 0;
	}    
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_detach_get(fspr_threadattr_t *attr)
{
	if (attr->detached == 1){
		return APR_DETACH;
	}
	return APR_NOTDETACH;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_stacksize_set(fspr_threadattr_t *attr,
                                                       fspr_size_t stacksize)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_threadattr_guardsize_set(fspr_threadattr_t *attr,
                                                       fspr_size_t size)
{
    return APR_ENOTIMPL;
}

static void *dummy_worker(void *opaque)
{
    fspr_thread_t *thd = (fspr_thread_t*)opaque;
    return thd->func(thd, thd->data);
}

APR_DECLARE(fspr_status_t) fspr_thread_create(fspr_thread_t **new, fspr_threadattr_t *attr,
                                            fspr_thread_start_t func, void *data,
                                            fspr_pool_t *pool)
{
    int32 temp;
    fspr_status_t stat;
    
    (*new) = (fspr_thread_t *)fspr_palloc(pool, sizeof(fspr_thread_t));
    if ((*new) == NULL) {
        return APR_ENOMEM;
    }

    (*new)->pool = pool;
    (*new)->data = data;
    (*new)->func = func;
    (*new)->exitval = -1;

    /* First we create the new thread...*/
	if (attr)
	    temp = attr->attr;
	else
	    temp = B_NORMAL_PRIORITY;

    stat = fspr_pool_create(&(*new)->pool, pool);
    if (stat != APR_SUCCESS) {
        return stat;
    }

    (*new)->td = spawn_thread((thread_func)dummy_worker, 
                              "apr thread", 
                              temp, 
                              (*new));

    /* Now we try to run it...*/
    if (resume_thread((*new)->td) == B_NO_ERROR) {
        return APR_SUCCESS;
    }
    else {
        return errno;
    } 
}

APR_DECLARE(fspr_os_thread_t) fspr_os_thread_current(void)
{
    return find_thread(NULL);
}

int fspr_os_thread_equal(fspr_os_thread_t tid1, fspr_os_thread_t tid2)
{
    return tid1 == tid2;
}

APR_DECLARE(fspr_status_t) fspr_thread_exit(fspr_thread_t *thd, fspr_status_t retval)
{
    fspr_pool_destroy(thd->pool);
    thd->exitval = retval;
    exit_thread ((status_t)(retval));
    /* This will never be reached... */
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_join(fspr_status_t *retval, fspr_thread_t *thd)
{
    status_t rv = 0, ret;
    ret = wait_for_thread(thd->td, &rv);
    if (ret == B_NO_ERROR) {
        *retval = rv;
        return APR_SUCCESS;
    }
    else {
        /* if we've missed the thread's death, did we set an exit value prior
         * to it's demise?  If we did return that.
         */
        if (thd->exitval != -1) {
            *retval = thd->exitval;
            return APR_SUCCESS;
        } else 
            return ret;
    }
}

APR_DECLARE(fspr_status_t) fspr_thread_detach(fspr_thread_t *thd)
{
	if (suspend_thread(thd->td) == B_NO_ERROR){
        return APR_SUCCESS;
    }
    else {
        return errno;
    }
}

void fspr_thread_yield()
{
}

APR_DECLARE(fspr_status_t) fspr_thread_data_get(void **data, const char *key, fspr_thread_t *thread)
{
    return fspr_pool_userdata_get(data, key, thread->pool);
}

APR_DECLARE(fspr_status_t) fspr_thread_data_set(void *data, const char *key,
                                              fspr_status_t (*cleanup) (void *),
                                              fspr_thread_t *thread)
{
    return fspr_pool_userdata_set(data, key, cleanup, thread->pool);
}

APR_DECLARE(fspr_status_t) fspr_os_thread_get(fspr_os_thread_t **thethd, fspr_thread_t *thd)
{
    *thethd = &thd->td;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_thread_put(fspr_thread_t **thd, fspr_os_thread_t *thethd, 
                                            fspr_pool_t *pool)
{
    if (pool == NULL) {
        return APR_ENOPOOL;
    }
    if ((*thd) == NULL) {
        (*thd) = (fspr_thread_t *)fspr_pcalloc(pool, sizeof(fspr_thread_t));
        (*thd)->pool = pool;
    }
    (*thd)->td = *thethd;
    return APR_SUCCESS;
}

static fspr_status_t thread_once_cleanup(void *vcontrol)
{
    fspr_thread_once_t *control = (fspr_thread_once_t *)vcontrol;

    if (control->sem) {
        release_sem(control->sem);
        delete_sem(control->sem);
    }

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_thread_once_init(fspr_thread_once_t **control,
                                               fspr_pool_t *p)
{
    int rc;
    *control = (fspr_thread_once_t *)fspr_pcalloc(p, sizeof(fspr_thread_once_t));
    (*control)->hit = 0; /* we haven't done it yet... */
    rc = ((*control)->sem = create_sem(1, "thread_once"));
    if (rc < 0)
        return rc;

    fspr_pool_cleanup_register(p, control, thread_once_cleanup, fspr_pool_cleanup_null);
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_thread_once(fspr_thread_once_t *control, 
                                          void (*func)(void))
{
    if (!control->hit) {
        if (acquire_sem(control->sem) == B_OK) {
            control->hit = 1;
            func();
        }
    }
    return APR_SUCCESS;
}

APR_POOL_IMPLEMENT_ACCESSOR(thread)
