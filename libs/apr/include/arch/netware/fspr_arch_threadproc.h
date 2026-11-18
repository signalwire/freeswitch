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
#include "fspr_thread_proc.h"
#include "fspr_file_io.h"

#include <sys/wait.h>

#ifndef THREAD_PROC_H
#define THREAD_PROC_H

#define SHELL_PATH ""
#define APR_DEFAULT_STACK_SIZE 65536

struct fspr_thread_t {
    fspr_pool_t *pool;
    NXContext_t ctx;
    NXThreadId_t td;
    char *thread_name;
    fspr_int32_t cancel;
    fspr_int32_t cancel_how;
    void *data;
    fspr_thread_start_t func;
    fspr_status_t exitval;
};

struct fspr_threadattr_t {
    fspr_pool_t *pool;
    fspr_size_t  stack_size;
    fspr_int32_t detach;
    char *thread_name;
};

struct fspr_threadkey_t {
    fspr_pool_t *pool;
    NXKey_t key;
};

struct fspr_procattr_t {
    fspr_pool_t *pool;
    fspr_file_t *parent_in;
    fspr_file_t *child_in;
    fspr_file_t *parent_out;
    fspr_file_t *child_out;
    fspr_file_t *parent_err;
    fspr_file_t *child_err;
    char *currdir;
    fspr_int32_t cmdtype;
    fspr_int32_t detached;
    fspr_int32_t addrspace;
};

struct fspr_thread_once_t {
    unsigned long value;
};

//struct fspr_proc_t {
//    fspr_pool_t *pool;
//    pid_t pid;
//    fspr_procattr_t *attr;
//};

#endif  /* ! THREAD_PROC_H */

