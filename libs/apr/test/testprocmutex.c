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

#include "fspr_shm.h"
#include "fspr_thread_proc.h"
#include "fspr_file_io.h"
#include "fspr_proc_mutex.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_getopt.h"
#include "errno.h"
#include <stdio.h>
#include <stdlib.h>
#include "testutil.h"

#if APR_HAS_FORK

#define MAX_ITER 200
#define CHILDREN 6
#define MAX_COUNTER (MAX_ITER * CHILDREN)

static fspr_proc_mutex_t *proc_lock;
static volatile int *x;

/* a slower more racy way to implement (*x)++ */
static int increment(int n)
{
    fspr_sleep(1);
    return n+1;
}

static void make_child(abts_case *tc, fspr_proc_t **proc, fspr_pool_t *p)
{
    fspr_status_t rv;

    *proc = fspr_pcalloc(p, sizeof(**proc));

    /* slight delay to allow things to settle */
    fspr_sleep (1);

    rv = fspr_proc_fork(*proc, p);
    if (rv == APR_INCHILD) {
        int i = 0;
        /* The parent process has setup all processes to call fspr_terminate
         * at exit.  But, that means that all processes must also call
         * fspr_initialize at startup.  You cannot have an unequal number
         * of fspr_terminate and fspr_initialize calls.  If you do, bad things
         * will happen.  In this case, the bad thing is that if the mutex
         * is a semaphore, it will be destroyed before all of the processes
         * die.  That means that the test will most likely fail.
         */
        fspr_initialize();

        if (fspr_proc_mutex_child_init(&proc_lock, NULL, p))
            exit(1);

        do {
            if (fspr_proc_mutex_lock(proc_lock))
                exit(1);
            i++;
            *x = increment(*x);
            if (fspr_proc_mutex_unlock(proc_lock))
                exit(1);
        } while (i < MAX_ITER);
        exit(0);
    } 

    ABTS_ASSERT(tc, "fork failed", rv == APR_INPARENT);
}

/* Wait for a child process and check it terminated with success. */
static void await_child(abts_case *tc, fspr_proc_t *proc)
{
    int code;
    fspr_exit_why_e why;
    fspr_status_t rv;

    rv = fspr_proc_wait(proc, &code, &why, APR_WAIT);
    ABTS_ASSERT(tc, "child did not terminate with success",
             rv == APR_CHILD_DONE && why == APR_PROC_EXIT && code == 0);
}

static void test_exclusive(abts_case *tc, const char *lockname, 
                           fspr_lockmech_e mech)
{
    fspr_proc_t *child[CHILDREN];
    fspr_status_t rv;
    int n;
 
    rv = fspr_proc_mutex_create(&proc_lock, lockname, mech, p);
    APR_ASSERT_SUCCESS(tc, "create the mutex", rv);
    if (rv != APR_SUCCESS)
        return;
 
    for (n = 0; n < CHILDREN; n++)
        make_child(tc, &child[n], p);

    for (n = 0; n < CHILDREN; n++)
        await_child(tc, child[n]);
    
    ABTS_ASSERT(tc, "Locks don't appear to work", *x == MAX_COUNTER);
}
#endif

static void proc_mutex(abts_case *tc, void *data)
{
#if APR_HAS_FORK
    fspr_status_t rv;
    const char *shmname = "tpm.shm";
    fspr_shm_t *shm;
    fspr_lockmech_e *mech = data;

    /* Use anonymous shm if available. */
    rv = fspr_shm_create(&shm, sizeof(int), NULL, p);
    if (rv == APR_ENOTIMPL) {
        fspr_file_remove(shmname, p);
        rv = fspr_shm_create(&shm, sizeof(int), shmname, p);
    }

    APR_ASSERT_SUCCESS(tc, "create shm segment", rv);
    if (rv != APR_SUCCESS)
        return;

    x = fspr_shm_baseaddr_get(shm);
    test_exclusive(tc, NULL, *mech);
    rv = fspr_shm_destroy(shm);
    APR_ASSERT_SUCCESS(tc, "Error destroying shared memory block", rv);
#else
    ABTS_NOT_IMPL(tc, "APR lacks fork() support");
#endif
}


abts_suite *testprocmutex(abts_suite *suite)
{
    fspr_lockmech_e mech = APR_LOCK_DEFAULT;

    suite = ADD_SUITE(suite)
    abts_run_test(suite, proc_mutex, &mech);
#if APR_HAS_POSIXSEM_SERIALIZE
    mech = APR_LOCK_POSIXSEM;
    abts_run_test(suite, proc_mutex, &mech);
#endif
#if APR_HAS_SYSVSEM_SERIALIZE
    mech = APR_LOCK_SYSVSEM;
    abts_run_test(suite, proc_mutex, &mech);
#endif
#if APR_HAS_PROC_PTHREAD_SERIALIZE
    mech = APR_LOCK_PROC_PTHREAD;
    abts_run_test(suite, proc_mutex, &mech);
#endif
#if APR_HAS_FCNTL_SERIALIZE
    mech = APR_LOCK_FCNTL;
    abts_run_test(suite, proc_mutex, &mech);
#endif
#if APR_HAS_FLOCK_SERIALIZE
    mech = APR_LOCK_FLOCK;
    abts_run_test(suite, proc_mutex, &mech);
#endif

    return suite;
}
