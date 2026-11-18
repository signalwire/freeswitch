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

#ifndef PROC_MUTEX_H
#define PROC_MUTEX_H

#ifndef __sun
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#endif

#include "fspr.h"
#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_proc_mutex.h"
#include "fspr_pools.h"
#include "fspr_portable.h"
#include "fspr_file_io.h"
#include "fspr_arch_file_io.h"

/* System headers required by Locks library */
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SYS_IPC_H
#include <sys/ipc.h>
#endif
#ifdef HAVE_SYS_SEM_H
#include <sys/sem.h>
#endif
#ifdef HAVE_SYS_FILE_H
#include <sys/file.h>
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#if APR_HAVE_PTHREAD_H
#include <pthread.h>
#endif
#if APR_HAVE_SEMAPHORE_H
#include <semaphore.h>
#endif
/* End System Headers */

struct fspr_proc_mutex_unix_lock_methods_t {
    unsigned int flags;
    fspr_status_t (*create)(fspr_proc_mutex_t *, const char *);
    fspr_status_t (*acquire)(fspr_proc_mutex_t *);
    fspr_status_t (*tryacquire)(fspr_proc_mutex_t *);
    fspr_status_t (*release)(fspr_proc_mutex_t *);
    fspr_status_t (*cleanup)(void *);
    fspr_status_t (*child_init)(fspr_proc_mutex_t **, fspr_pool_t *, const char *);
    const char *name;
};
typedef struct fspr_proc_mutex_unix_lock_methods_t fspr_proc_mutex_unix_lock_methods_t;

/* bit values for flags field in fspr_unix_lock_methods_t */
#define APR_PROCESS_LOCK_MECH_IS_GLOBAL          1

#if !APR_HAVE_UNION_SEMUN && defined(APR_HAS_SYSVSEM_SERIALIZE)
union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};
#endif

struct fspr_proc_mutex_t {
    fspr_pool_t *pool;
    const fspr_proc_mutex_unix_lock_methods_t *meth;
    const fspr_proc_mutex_unix_lock_methods_t *inter_meth;
    int curr_locked;
    char *fname;
#if APR_HAS_SYSVSEM_SERIALIZE || APR_HAS_FCNTL_SERIALIZE || APR_HAS_FLOCK_SERIALIZE
    fspr_file_t *interproc;
#endif
#if APR_HAS_POSIXSEM_SERIALIZE
    sem_t *psem_interproc;
#endif
#if APR_HAS_PROC_PTHREAD_SERIALIZE
    pthread_mutex_t *pthread_interproc;
#endif
};

void fspr_proc_mutex_unix_setup_lock(void);

#endif  /* PROC_MUTEX_H */

