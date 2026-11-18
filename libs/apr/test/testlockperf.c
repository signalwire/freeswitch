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
#include "fspr_thread_mutex.h"
#include "fspr_thread_rwlock.h"
#include "fspr_file_io.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_getopt.h"
#include "errno.h"
#include <stdio.h>
#include <stdlib.h>
#include "testutil.h"

#if !APR_HAS_THREADS
int main(void)
{
    printf("This program won't work on this platform because there is no "
           "support for threads.\n");
    return 0;
}
#else /* !APR_HAS_THREADS */

#define MAX_COUNTER 1000000
#define MAX_THREADS 6

static long mutex_counter;

static fspr_thread_mutex_t *thread_lock;
void * APR_THREAD_FUNC thread_mutex_func(fspr_thread_t *thd, void *data);
fspr_status_t test_thread_mutex(int num_threads); /* fspr_thread_mutex_t */

static fspr_thread_rwlock_t *thread_rwlock;
void * APR_THREAD_FUNC thread_rwlock_func(fspr_thread_t *thd, void *data);
fspr_status_t test_thread_rwlock(int num_threads); /* fspr_thread_rwlock_t */

int test_thread_mutex_nested(int num_threads);

fspr_pool_t *pool;
int i = 0, x = 0;

void * APR_THREAD_FUNC thread_mutex_func(fspr_thread_t *thd, void *data)
{
    int i;

    for (i = 0; i < MAX_COUNTER; i++) {
        fspr_thread_mutex_lock(thread_lock);
        mutex_counter++;
        fspr_thread_mutex_unlock(thread_lock);
    }
    return NULL;
}

void * APR_THREAD_FUNC thread_rwlock_func(fspr_thread_t *thd, void *data)
{
    int i;

    for (i = 0; i < MAX_COUNTER; i++) {
        fspr_thread_rwlock_wrlock(thread_rwlock);
        mutex_counter++;
        fspr_thread_rwlock_unlock(thread_rwlock);
    }
    return NULL;
}

int test_thread_mutex(int num_threads)
{
    fspr_thread_t *t[MAX_THREADS];
    fspr_status_t s[MAX_THREADS];
    fspr_time_t time_start, time_stop;
    int i;

    mutex_counter = 0;

    printf("fspr_thread_mutex_t Tests\n");
    printf("%-60s", "    Initializing the fspr_thread_mutex_t (UNNESTED)");
    s[0] = fspr_thread_mutex_create(&thread_lock, APR_THREAD_MUTEX_UNNESTED, pool);
    if (s[0] != APR_SUCCESS) {
        printf("Failed!\n");
        return s[0];
    }
    printf("OK\n");

    fspr_thread_mutex_lock(thread_lock);
    /* set_concurrency(4)? -aaron */
    printf("    Starting %d threads    ", num_threads); 
    for (i = 0; i < num_threads; ++i) {
        s[i] = fspr_thread_create(&t[i], NULL, thread_mutex_func, NULL, pool);
        if (s[i] != APR_SUCCESS) {
            printf("Failed!\n");
            return s[i];
        }
    }
    printf("OK\n");

    time_start = fspr_time_now();
    fspr_thread_mutex_unlock(thread_lock);

    /* printf("%-60s", "    Waiting for threads to exit"); */
    for (i = 0; i < num_threads; ++i) {
        fspr_thread_join(&s[i], t[i]);
    }
    /* printf("OK\n"); */

    time_stop = fspr_time_now();
    printf("microseconds: %" APR_INT64_T_FMT " usec\n",
           (time_stop - time_start));
    if (mutex_counter != MAX_COUNTER * num_threads)
        printf("error: counter = %ld\n", mutex_counter);

    return APR_SUCCESS;
}

int test_thread_mutex_nested(int num_threads)
{
    fspr_thread_t *t[MAX_THREADS];
    fspr_status_t s[MAX_THREADS];
    fspr_time_t time_start, time_stop;
    int i;

    mutex_counter = 0;

    printf("fspr_thread_mutex_t Tests\n");
    printf("%-60s", "    Initializing the fspr_thread_mutex_t (NESTED)");
    s[0] = fspr_thread_mutex_create(&thread_lock, APR_THREAD_MUTEX_NESTED, pool);
    if (s[0] != APR_SUCCESS) {
        printf("Failed!\n");
        return s[0];
    }
    printf("OK\n");

    fspr_thread_mutex_lock(thread_lock);
    /* set_concurrency(4)? -aaron */
    printf("    Starting %d threads    ", num_threads); 
    for (i = 0; i < num_threads; ++i) {
        s[i] = fspr_thread_create(&t[i], NULL, thread_mutex_func, NULL, pool);
        if (s[i] != APR_SUCCESS) {
            printf("Failed!\n");
            return s[i];
        }
    }
    printf("OK\n");

    time_start = fspr_time_now();
    fspr_thread_mutex_unlock(thread_lock);

    /* printf("%-60s", "    Waiting for threads to exit"); */
    for (i = 0; i < num_threads; ++i) {
        fspr_thread_join(&s[i], t[i]);
    }
    /* printf("OK\n"); */

    time_stop = fspr_time_now();
    printf("microseconds: %" APR_INT64_T_FMT " usec\n",
           (time_stop - time_start));
    if (mutex_counter != MAX_COUNTER * num_threads)
        printf("error: counter = %ld\n", mutex_counter);

    return APR_SUCCESS;
}

int test_thread_rwlock(int num_threads)
{
    fspr_thread_t *t[MAX_THREADS];
    fspr_status_t s[MAX_THREADS];
    fspr_time_t time_start, time_stop;
    int i;

    mutex_counter = 0;

    printf("fspr_thread_rwlock_t Tests\n");
    printf("%-60s", "    Initializing the fspr_thread_rwlock_t");
    s[0] = fspr_thread_rwlock_create(&thread_rwlock, pool);
    if (s[0] != APR_SUCCESS) {
        printf("Failed!\n");
        return s[0];
    }
    printf("OK\n");

    fspr_thread_rwlock_wrlock(thread_rwlock);
    /* set_concurrency(4)? -aaron */
    printf("    Starting %d threads    ", num_threads); 
    for (i = 0; i < num_threads; ++i) {
        s[i] = fspr_thread_create(&t[i], NULL, thread_rwlock_func, NULL, pool);
        if (s[i] != APR_SUCCESS) {
            printf("Failed!\n");
            return s[i];
        }
    }
    printf("OK\n");

    time_start = fspr_time_now();
    fspr_thread_rwlock_unlock(thread_rwlock);

    /* printf("%-60s", "    Waiting for threads to exit"); */
    for (i = 0; i < num_threads; ++i) {
        fspr_thread_join(&s[i], t[i]);
    }
    /* printf("OK\n"); */

    time_stop = fspr_time_now();
    printf("microseconds: %" APR_INT64_T_FMT " usec\n",
           (time_stop - time_start));
    if (mutex_counter != MAX_COUNTER * num_threads)
        printf("error: counter = %ld\n", mutex_counter);

    return APR_SUCCESS;
}

int main(int argc, const char * const *argv)
{
    fspr_status_t rv;
    char errmsg[200];
    const char *lockname = "multi.lock";
    fspr_getopt_t *opt;
    char optchar;
    const char *optarg;

    printf("APR Lock Performance Test\n==============\n\n");
        
    fspr_initialize();
    atexit(fspr_terminate);

    if (fspr_pool_create(&pool, NULL) != APR_SUCCESS)
        exit(-1);

    if ((rv = fspr_getopt_init(&opt, pool, argc, argv)) != APR_SUCCESS) {
        fprintf(stderr, "Could not set up to parse options: [%d] %s\n",
                rv, fspr_strerror(rv, errmsg, sizeof errmsg));
        exit(-1);
    }
        
    while ((rv = fspr_getopt(opt, "f:", &optchar, &optarg)) == APR_SUCCESS) {
        if (optchar == 'f') {
            lockname = optarg;
        }
    }

    if (rv != APR_SUCCESS && rv != APR_EOF) {
        fprintf(stderr, "Could not parse options: [%d] %s\n",
                rv, fspr_strerror(rv, errmsg, sizeof errmsg));
        exit(-1);
    }

    for (i = 1; i <= MAX_THREADS; ++i) {
        if ((rv = test_thread_mutex(i)) != APR_SUCCESS) {
            fprintf(stderr,"thread_mutex test failed : [%d] %s\n",
                    rv, fspr_strerror(rv, (char*)errmsg, 200));
            exit(-3);
        }

        if ((rv = test_thread_mutex_nested(i)) != APR_SUCCESS) {
            fprintf(stderr,"thread_mutex (NESTED) test failed : [%d] %s\n",
                    rv, fspr_strerror(rv, (char*)errmsg, 200));
            exit(-4);
        }

        if ((rv = test_thread_rwlock(i)) != APR_SUCCESS) {
            fprintf(stderr,"thread_rwlock test failed : [%d] %s\n",
                    rv, fspr_strerror(rv, (char*)errmsg, 200));
            exit(-6);
        }
    }

    return 0;
}

#endif /* !APR_HAS_THREADS */
