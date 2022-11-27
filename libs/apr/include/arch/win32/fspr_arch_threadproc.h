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

#include "fspr_private.h"
#include "fspr_thread_proc.h"
#include "fspr_file_io.h"

#ifndef THREAD_PROC_H
#define THREAD_PROC_H

#define SHELL_PATH "cmd.exe"

struct fspr_thread_t {
    fspr_pool_t *pool;
    HANDLE td;
    fspr_int32_t cancel;
    fspr_int32_t cancel_how;
    void *data;
    fspr_thread_start_t func;
    fspr_status_t exitval;
};

struct fspr_threadattr_t {
    fspr_pool_t *pool;
    fspr_int32_t detach;
    fspr_size_t stacksize;
    int priority;
};

struct fspr_threadkey_t {
    fspr_pool_t *pool;
    DWORD key;
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
    fspr_child_errfn_t *errfn;
    fspr_int32_t errchk;
#ifndef _WIN32_WCE
    HANDLE     user_token;
    LPSECURITY_ATTRIBUTES   sa;
    LPVOID                  sd;
#endif
};

struct fspr_thread_once_t {
    long value;
};

#endif  /* ! THREAD_PROC_H */

