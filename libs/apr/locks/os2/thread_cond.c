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

#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "fspr_portable.h"
#include "fspr_arch_thread_mutex.h"
#include "fspr_arch_thread_cond.h"
#include "fspr_arch_file_io.h"
#include <string.h>

APR_DECLARE(fspr_status_t) fspr_thread_cond_create(fspr_thread_cond_t **cond,
                                                 fspr_pool_t *pool)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_wait(fspr_thread_cond_t *cond,
                                               fspr_thread_mutex_t *mutex)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_timedwait(fspr_thread_cond_t *cond,
                                                    fspr_thread_mutex_t *mutex,
                                                    fspr_interval_time_t timeout){
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_signal(fspr_thread_cond_t *cond)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_broadcast(fspr_thread_cond_t *cond)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_thread_cond_destroy(fspr_thread_cond_t *cond)
{
    return APR_ENOTIMPL;
}

APR_POOL_IMPLEMENT_ACCESSOR(thread_cond)

