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

#include "apr_shm.h"
#include "apr_rmm.h"
#include "apr_errno.h"
#include "apr_general.h"
#include "apr_lib.h"
#include "apr_strings.h"
#include "apr_time.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif

#if APR_HAS_SHARED_MEMORY

#define FRAG_SIZE 80
#define FRAG_COUNT 10
#define SHARED_SIZE (apr_size_t)(FRAG_SIZE * FRAG_COUNT * sizeof(char*))

static apr_status_t test_rmm(apr_pool_t *parpool)
{
    apr_status_t rv;
    apr_pool_t *pool;
    apr_shm_t *shm;
    apr_rmm_t *rmm;
    apr_size_t size, fragsize;
    apr_rmm_off_t *off;
    int i;
    void *entity;

    rv = apr_pool_create(&pool, parpool);
    if (rv != APR_SUCCESS) {
        fprintf(stderr, "Error creating child pool\n");
        return rv;
    }

    /* We're going to want 10 blocks of data from our target rmm. */
    size = SHARED_SIZE + apr_rmm_overhead_get(FRAG_COUNT + 1);
    printf("Creating anonymous shared memory (%"
           APR_SIZE_T_FMT " bytes).....", size); 
    rv = apr_shm_create(&shm, size, NULL, pool);
    if (rv != APR_SUCCESS) { 
        fprintf(stderr, "Error allocating shared memory block\n");
        return rv;
    }
    fprintf(stdout, "OK\n");

    printf("Creating rmm segment.............................");
    rv = apr_rmm_init(&rmm, NULL, apr_shm_baseaddr_get(shm), size,
                      pool);

    if (rv != APR_SUCCESS) {
        fprintf(stderr, "Error allocating rmm..............\n");
        return rv;
    }
    fprintf(stdout, "OK\n");

    fragsize = SHARED_SIZE / FRAG_COUNT;
    printf("Creating each fragment of size %" APR_SIZE_T_FMT "................",
           fragsize);
    off = apr_palloc(pool, FRAG_COUNT * sizeof(apr_rmm_off_t));
    for (i = 0; i < FRAG_COUNT; i++) {
        off[i] = apr_rmm_malloc(rmm, fragsize);
    } 
    fprintf(stdout, "OK\n");
    
    printf("Checking for out of memory allocation............");
    if (apr_rmm_malloc(rmm, FRAG_SIZE * FRAG_COUNT) == 0) {
        fprintf(stdout, "OK\n");
    }
    else {
        return APR_EGENERAL;  
    }

    printf("Checking each fragment for address alignment.....");
    for (i = 0; i < FRAG_COUNT; i++) {
        char *c = apr_rmm_addr_get(rmm, off[i]);
        apr_size_t sc = (apr_size_t)c;

        if (off[i] == 0) {
            printf("allocation failed for offset %d\n", i);
            return APR_ENOMEM;
        }

        if (sc & 7) {
            printf("Bad alignment for fragment %d; %p not %p!\n",
                   i, c, (void *)APR_ALIGN_DEFAULT((apr_size_t)c));
            return APR_EGENERAL;
        }
    }
    fprintf(stdout, "OK\n");   
    
    printf("Setting each fragment to a unique value..........");
    for (i = 0; i < FRAG_COUNT; i++) {
        int j;
        char **c = apr_rmm_addr_get(rmm, off[i]);
        for (j = 0; j < FRAG_SIZE; j++, c++) {
            *c = apr_itoa(pool, i + j);
        }
    } 
    fprintf(stdout, "OK\n");

    printf("Checking each fragment for its unique value......");
    for (i = 0; i < FRAG_COUNT; i++) {
        int j;
        char **c = apr_rmm_addr_get(rmm, off[i]);
        for (j = 0; j < FRAG_SIZE; j++, c++) {
            char *d = apr_itoa(pool, i + j);
            if (strcmp(*c, d) != 0) {
                return APR_EGENERAL;
            }
        }
    } 
    fprintf(stdout, "OK\n");

    printf("Freeing each fragment............................");
    for (i = 0; i < FRAG_COUNT; i++) {
        rv = apr_rmm_free(rmm, off[i]);
        if (rv != APR_SUCCESS) {
            return rv;
        }
    } 
    fprintf(stdout, "OK\n");

    printf("Creating one large segment.......................");
    off[0] = apr_rmm_calloc(rmm, SHARED_SIZE);
    fprintf(stdout, "OK\n");

    printf("Setting large segment............................");
    for (i = 0; i < FRAG_COUNT * FRAG_SIZE; i++) {
        char **c = apr_rmm_addr_get(rmm, off[0]);
        c[i] = apr_itoa(pool, i);
    }
    fprintf(stdout, "OK\n");

    printf("Freeing large segment............................");
    apr_rmm_free(rmm, off[0]);
    fprintf(stdout, "OK\n");

    printf("Creating each fragment of size %" APR_SIZE_T_FMT " (again)........",
           fragsize);
    for (i = 0; i < FRAG_COUNT; i++) {
        off[i] = apr_rmm_malloc(rmm, fragsize);
    } 
    fprintf(stdout, "OK\n");

    printf("Freeing each fragment backwards..................");
    for (i = FRAG_COUNT - 1; i >= 0; i--) {
        rv = apr_rmm_free(rmm, off[i]);
        if (rv != APR_SUCCESS) {
            return rv;
        }
    } 
    fprintf(stdout, "OK\n");

    printf("Creating one large segment (again)...............");
    off[0] = apr_rmm_calloc(rmm, SHARED_SIZE);
    fprintf(stdout, "OK\n");

    printf("Freeing large segment............................");
    apr_rmm_free(rmm, off[0]);
    fprintf(stdout, "OK\n");

    printf("Checking realloc.................................");
    off[0] = apr_rmm_calloc(rmm, SHARED_SIZE - 100);
    off[1] = apr_rmm_calloc(rmm, 100);
    if (off[0] == 0 || off[1] == 0) {
        printf("FAILED\n");
        return APR_EINVAL;
    }
    entity = apr_rmm_addr_get(rmm, off[1]);
    rv = apr_rmm_free(rmm, off[0]);
    if (rv != APR_SUCCESS) {
        printf("FAILED\n");
        return rv;
    }

    {
        unsigned char *c = entity;

        /* Fill in the region; the first half with zereos, which will
         * likely catch the apr_rmm_realloc offset calculation bug by
         * making it think the old region was zero length. */
        for (i = 0; i < 100; i++) {
            c[i] = (i < 50) ? 0 : i;
        }
    }

    /* now we can realloc off[1] and get many more bytes */
    off[0] = apr_rmm_realloc(rmm, entity, SHARED_SIZE - 100);
    if (off[0] == 0) {
        printf("FAILED\n");
        return APR_EINVAL;
    }

    {
        unsigned char *c = apr_rmm_addr_get(rmm, off[0]);

        /* fill in the region */
        for (i = 0; i < 100; i++) {
            if (c[i] != (i < 50 ? 0 : i)) {
                printf("FAILED at offset %d: %hx\n", i, c[i]);
                return APR_EGENERAL;
            }
        }
    }

    fprintf(stdout, "OK\n");

    printf("Destroying rmm segment...........................");
    rv = apr_rmm_destroy(rmm);
    if (rv != APR_SUCCESS) {
        printf("FAILED\n");
        return rv;
    }
    printf("OK\n");

    printf("Destroying shared memory segment.................");
    rv = apr_shm_destroy(shm);
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
    
    printf("APR RMM Memory Test\n");
    printf("======================\n\n");

    printf("Initializing the pool............................"); 
    if (apr_pool_create(&pool, NULL) != APR_SUCCESS) {
        printf("could not initialize pool\n");
        exit(-1);
    }
    printf("OK\n");

    rv = test_rmm(pool);
    if (rv != APR_SUCCESS) {
        printf("Anonymous shared memory test FAILED: [%d] %s\n",
               rv, apr_strerror(rv, errmsg, sizeof(errmsg)));
        exit(-2);
    }
    printf("RMM test passed!\n");

    return 0;
}

#else /* APR_HAS_SHARED_MEMORY */
#error shmem is not supported on this platform
#endif /* APR_HAS_SHARED_MEMORY */

