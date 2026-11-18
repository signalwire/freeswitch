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

#include "fspr_thread_proc.h"
#include "fspr_arch_file_io.h"
#include "fspr_file_io.h"
#include "fspr_thread_proc.h"
#include "fspr_general.h"
#include "fspr_portable.h"
#include <kernel/OS.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <image.h>

#ifndef THREAD_PROC_H
#define THREAD_PROC_H

#define SHELL_PATH "/bin/sh"

#define PTHREAD_CANCEL_AYNCHRONOUS  CANCEL_ASYNCH; 
#define PTHREAD_CANCEL_DEFERRED     CANCEL_DEFER; 
                                   
#define PTHREAD_CANCEL_ENABLE       CANCEL_ENABLE; 
#define PTHREAD_CANCEL_DISABLE      CANCEL_DISABLE; 

#define BEOS_MAX_DATAKEYS	128

struct fspr_thread_t {
    fspr_pool_t *pool;
    thread_id td;
    void *data;
    fspr_thread_start_t func;
    fspr_status_t exitval;
};

struct fspr_threadattr_t {
    fspr_pool_t *pool;
    int32 attr;
    int detached;
    int joinable;
};

struct fspr_threadkey_t {
    fspr_pool_t *pool;
	int32  key;
};

struct beos_private_data {
	const void ** data;
	int count;
	volatile thread_id  td;
};

struct beos_key {
	int  assigned;
	int  count;
	sem_id  lock;
	int32  ben_lock;
	void (* destructor) (void *);
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
};

struct fspr_thread_once_t {
    sem_id sem;
    int hit;
};

#endif  /* ! THREAD_PROC_H */

