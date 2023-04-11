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

#ifndef THREAD_RWLOCK_H
#define THREAD_RWLOCK_H

#include <kernel/OS.h>
#include "fspr_pools.h"
#include "fspr_thread_rwlock.h"
#include "fspr_file_io.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_portable.h"

struct fspr_thread_rwlock_t {
    fspr_pool_t *pool;

    /* Our lock :) */
    sem_id Lock;
    int32  LockCount;
    /* Read/Write lock stuff */
    sem_id Read;
    int32  ReadCount;
    sem_id Write;
    int32  WriteCount;
    int32  Nested;

    thread_id writer;
};

#endif  /* THREAD_RWLOCK_H */

