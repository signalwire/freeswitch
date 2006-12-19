/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include "apr_reslist.h"
#include "apr_thread_proc.h"

#if APR_HAVE_TIME_H
#include <time.h>
#endif /* APR_HAVE_TIME_H */

#if !APR_HAS_THREADS

int main(void)
{
    fprintf(stderr, "this program requires APR thread support\n");
    return 0;
}

#else

#define RESLIST_MIN   3
#define RESLIST_SMAX 10
#define RESLIST_HMAX 20
#define RESLIST_TTL  APR_TIME_C(350000) /* 35 ms */
#define CONSUMER_THREADS 25
#define CONSUMER_ITERATIONS 250
#define CONSTRUCT_SLEEP_TIME  APR_TIME_C(250000) /* 25 ms */
#define DESTRUCT_SLEEP_TIME   APR_TIME_C(100000) /* 10 ms */
#define WORK_DELAY_SLEEP_TIME APR_TIME_C(150000) /* 15 ms */

typedef struct {
    apr_interval_time_t sleep_upon_construct;
    apr_interval_time_t sleep_upon_destruct;
    int c_count;
    int d_count;
} my_parameters_t;

typedef struct {
    int id;
} my_resource_t;

static apr_status_t my_constructor(void **resource, void *params,
                                   apr_pool_t *pool)
{
    my_resource_t *res;
    my_parameters_t *my_params = params;

    /* Create some resource */
    res = apr_palloc(pool, sizeof(*res));
    res->id = my_params->c_count++;

    printf("++ constructing new resource [id:%d, #%d/%d]\n", res->id,
       my_params->c_count, my_params->d_count);

    /* Sleep for awhile, to simulate construction overhead. */
    apr_sleep(my_params->sleep_upon_construct);

    /* Set the resource so it can be managed by the reslist */
    *resource = res;
    return APR_SUCCESS;
}

static apr_status_t my_destructor(void *resource, void *params,
                                  apr_pool_t *pool)
{
    my_resource_t *res = resource;
    my_parameters_t *my_params = params;

    printf("-- destructing old resource [id:%d, #%d/%d]\n", res->id,
           my_params->c_count, ++my_params->d_count);

    apr_sleep(my_params->sleep_upon_destruct);

    return APR_SUCCESS;
}

typedef struct {
    int tid;
    apr_reslist_t *reslist;
    apr_interval_time_t work_delay_sleep;
} my_thread_info_t;

static void * APR_THREAD_FUNC resource_consuming_thread(apr_thread_t *thd,
                                                        void *data)
{
    apr_status_t rv;
    my_thread_info_t *thread_info = data;
    apr_reslist_t *rl = thread_info->reslist;
    int i;

    for (i = 0; i < CONSUMER_ITERATIONS; i++) {
        my_resource_t *res;
        void *vp;
        rv = apr_reslist_acquire(rl, &vp);
        if (rv != APR_SUCCESS) {
            fprintf(stderr, "Failed to retrieve resource from reslist\n");
            apr_thread_exit(thd, rv);
            return NULL;
        }
        res = vp;
        printf("  [tid:%d,iter:%d] using resource id:%d\n", thread_info->tid,
               i, res->id);
        apr_sleep(thread_info->work_delay_sleep);
/* simulate a 5% chance of the resource being bad */
        if ( drand48() < 0.95 ) {
           rv = apr_reslist_release(rl, res);
            if (rv != APR_SUCCESS) {
                fprintf(stderr, "Failed to return resource to reslist\n");
                apr_thread_exit(thd, rv);
                return NULL;
            }
       } else {
           printf("invalidating resource id:%d\n", res->id) ;
           rv = apr_reslist_invalidate(rl, res);
            if (rv != APR_SUCCESS) {
                fprintf(stderr, "Failed to invalidate resource\n");
                apr_thread_exit(thd, rv);
                return NULL;
            }
       }
    }

    return APR_SUCCESS;
}

static void test_timeout(apr_reslist_t *rl)
{
    apr_status_t rv;
    my_resource_t *resources[RESLIST_HMAX];
    my_resource_t *res;
    void *vp;
    int i;

    printf("Setting timeout to 1000us: ");
    apr_reslist_timeout_set(rl, 1000);
    fprintf(stdout, "OK\n");

    /* deplete all possible resources from the resource list 
     * so that the next call will block until timeout is reached 
     * (since there are no other threads to make a resource 
     * available)
     */

    for (i = 0; i < RESLIST_HMAX; i++) {
        rv = apr_reslist_acquire(rl, (void**)&resources[i]);
        if (rv != APR_SUCCESS) {
            fprintf(stderr, "couldn't acquire resource: %d\n", rv);
            exit(1);
        }
    }

    /* next call will block until timeout is reached */
    rv = apr_reslist_acquire(rl, &vp);
    if (!APR_STATUS_IS_TIMEUP(rv)) {
        fprintf(stderr, "apr_reslist_acquire()->%d instead of TIMEUP\n", 
                rv);
        exit(1);
    }
    res = vp;

    /* release the resources; otherwise the destroy operation
     * will blow
     */
    for (i = 0; i < RESLIST_HMAX; i++) {
        rv = apr_reslist_release(rl, &resources[i]);
        if (rv != APR_SUCCESS) {
            fprintf(stderr, "couldn't release resource: %d\n", rv);
            exit(1);
        }
    }
}

static apr_status_t test_reslist(apr_pool_t *parpool)
{
    apr_status_t rv;
    apr_pool_t *pool;
    apr_reslist_t *rl;
    my_parameters_t *params;
    int i;
    apr_thread_t *my_threads[CONSUMER_THREADS];
    my_thread_info_t my_thread_info[CONSUMER_THREADS];
    srand48(time(0)) ;

    printf("Creating child pool.......................");
    rv = apr_pool_create(&pool, parpool);
    if (rv != APR_SUCCESS) {
        fprintf(stderr, "Error creating child pool\n");
        return rv;
    }
    printf("OK\n");

    /* Create some parameters that will be passed into each
     * constructor and destructor call. */
    params = apr_pcalloc(pool, sizeof(*params));
    params->sleep_upon_construct = CONSTRUCT_SLEEP_TIME;
    params->sleep_upon_destruct = DESTRUCT_SLEEP_TIME;

    /* We're going to want 10 blocks of data from our target rmm. */
    printf("Creating resource list:\n"
           " min/smax/hmax: %d/%d/%d\n"
           " ttl: %" APR_TIME_T_FMT "\n", RESLIST_MIN, RESLIST_SMAX,
           RESLIST_HMAX, RESLIST_TTL);
    rv = apr_reslist_create(&rl, RESLIST_MIN, RESLIST_SMAX, RESLIST_HMAX,
                            RESLIST_TTL, my_constructor, my_destructor,
                            params, pool);
    if (rv != APR_SUCCESS) { 
        fprintf(stderr, "Error allocating shared memory block\n");
        return rv;
    }
    fprintf(stdout, "OK\n");

    printf("Creating %d threads", CONSUMER_THREADS);
    for (i = 0; i < CONSUMER_THREADS; i++) {
        putchar('.');
        my_thread_info[i].tid = i;
        my_thread_info[i].reslist = rl;
        my_thread_info[i].work_delay_sleep = WORK_DELAY_SLEEP_TIME;
        rv = apr_thread_create(&my_threads[i], NULL,
                               resource_consuming_thread, &my_thread_info[i],
                               pool);
        if (rv != APR_SUCCESS) {
            fprintf(stderr, "Failed to create thread %d\n", i);
            return rv;
        }
    }
    printf("\nDone!\n");

    printf("Waiting for threads to finish");
    for (i = 0; i < CONSUMER_THREADS; i++) {
        apr_status_t thread_rv;
        putchar('.');
        apr_thread_join(&thread_rv, my_threads[i]);
        if (rv != APR_SUCCESS) {
            fprintf(stderr, "Failed to join thread %d\n", i);
            return rv;
        }
    }
    printf("\nDone!\n");

    test_timeout(rl);

    printf("Destroying resource list.................");
    rv = apr_reslist_destroy(rl);
    if (rv != APR_SUCCESS) {
        printf("FAILED\n");
        return rv;
    }
    printf("OK\n");

    apr_pool_destroy(pool);

    return APR_SUCCESS;
}


int main(void)
{
    apr_status_t rv;
    apr_pool_t *pool;
    char errmsg[200];

    apr_initialize();
    
    printf("APR Resource List Test\n");
    printf("======================\n\n");

    printf("Initializing the pool............................"); 
    if (apr_pool_create(&pool, NULL) != APR_SUCCESS) {
        printf("could not initialize pool\n");
        exit(-1);
    }
    printf("OK\n");

    rv = test_reslist(pool);
    if (rv != APR_SUCCESS) {
        printf("Resource list test FAILED: [%d] %s\n",
               rv, apr_strerror(rv, errmsg, sizeof(errmsg)));
        exit(-2);
    }
    printf("Resource list test passed!\n");

    return 0;
}

#endif /* APR_HAS_THREADS */
