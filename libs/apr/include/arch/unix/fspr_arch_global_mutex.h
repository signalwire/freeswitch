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

#ifndef GLOBAL_MUTEX_H
#define GLOBAL_MUTEX_H

#include "fspr.h"
#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_global_mutex.h"
#include "fspr_arch_proc_mutex.h"
#include "fspr_arch_thread_mutex.h"

struct fspr_global_mutex_t {
    fspr_pool_t *pool;
    fspr_proc_mutex_t *proc_mutex;
#if APR_HAS_THREADS
    fspr_thread_mutex_t *thread_mutex;
#endif /* APR_HAS_THREADS */
};

#endif  /* GLOBAL_MUTEX_H */

