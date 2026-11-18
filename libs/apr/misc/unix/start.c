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

#include "fspr.h"
#include "fspr_general.h"
#include "fspr_pools.h"
#include "fspr_signal.h"
#include "fspr_atomic.h"

#include "fspr_arch_proc_mutex.h" /* for fspr_proc_mutex_unix_setup_lock() */
#include "fspr_arch_internal_time.h"


APR_DECLARE(fspr_status_t) fspr_app_initialize(int *argc, 
                                             const char * const * *argv, 
                                             const char * const * *env)
{
    /* An absolute noop.  At present, only Win32 requires this stub, but it's
     * required in order to move command arguments passed through the service
     * control manager into the process, and it's required to fix the char*
     * data passed in from win32 unicode into utf-8, win32's apr internal fmt.
     */
    return fspr_initialize();
}

static int initialized = 0;

APR_DECLARE(fspr_status_t) fspr_initialize(void)
{
    fspr_pool_t *pool;
    fspr_status_t status;

    if (initialized++) {
        return APR_SUCCESS;
    }

#if !defined(BEOS) && !defined(OS2)
    fspr_proc_mutex_unix_setup_lock();
    fspr_unix_setup_time();
#endif

    if ((status = fspr_pool_initialize()) != APR_SUCCESS)
        return status;
    
    if (fspr_pool_create(&pool, NULL) != APR_SUCCESS) {
        return APR_ENOPOOL;
    }

    fspr_pool_tag(pool, "fspr_initialize");

    /* fspr_atomic_init() used to be called from here aswell.
     * Pools rely on mutexes though, which can be backed by
     * atomics.  Due to this circular dependency
     * fspr_pool_initialize() is taking care of calling
     * fspr_atomic_init() at the correct time.
     */

    fspr_signal_init(pool);

    return APR_SUCCESS;
}

APR_DECLARE_NONSTD(void) fspr_terminate(void)
{
    initialized--;
    if (initialized) {
        return;
    }
    fspr_pool_terminate();
    
}

APR_DECLARE(void) fspr_terminate2(void)
{
    fspr_terminate();
}
