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

#include "testutil.h"
#include "fspr_shm.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "fspr_thread_proc.h"
#include "fspr_time.h"
#include "testshm.h"
#include "fspr.h"

#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif

#if APR_HAS_SHARED_MEMORY

static int msgwait(int sleep_sec, int first_box, int last_box)
{
    int i;
    int recvd = 0;
    fspr_time_t start = fspr_time_now();
    fspr_interval_time_t sleep_duration = fspr_time_from_sec(sleep_sec);
    while (fspr_time_now() - start < sleep_duration) {
        for (i = first_box; i < last_box; i++) {
            if (boxes[i].msgavail && !strcmp(boxes[i].msg, MSG)) {
                recvd++;
                boxes[i].msgavail = 0; /* reset back to 0 */
                /* reset the msg field.  1024 is a magic number and it should
                 * be a macro, but I am being lazy.
                 */
                memset(boxes[i].msg, 0, 1024);
            }
        }
        fspr_sleep(fspr_time_make(0, 10000)); /* 10ms */
    }
    return recvd;
}

static void msgput(int boxnum, char *msg)
{
    fspr_cpystrn(boxes[boxnum].msg, msg, strlen(msg) + 1);
    boxes[boxnum].msgavail = 1;
}

static void test_anon_create(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_shm_t *shm = NULL;

    rv = fspr_shm_create(&shm, SHARED_SIZE, NULL, p);
    APR_ASSERT_SUCCESS(tc, "Error allocating shared memory block", rv);
    ABTS_PTR_NOTNULL(tc, shm);

    rv = fspr_shm_destroy(shm);
    APR_ASSERT_SUCCESS(tc, "Error destroying shared memory block", rv);
}

static void test_check_size(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_shm_t *shm = NULL;
    fspr_size_t retsize;

    rv = fspr_shm_create(&shm, SHARED_SIZE, NULL, p);
    APR_ASSERT_SUCCESS(tc, "Error allocating shared memory block", rv);
    ABTS_PTR_NOTNULL(tc, shm);

    retsize = fspr_shm_size_get(shm);
    ABTS_INT_EQUAL(tc, SHARED_SIZE, retsize);

    rv = fspr_shm_destroy(shm);
    APR_ASSERT_SUCCESS(tc, "Error destroying shared memory block", rv);
}

static void test_shm_allocate(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_shm_t *shm = NULL;

    rv = fspr_shm_create(&shm, SHARED_SIZE, NULL, p);
    APR_ASSERT_SUCCESS(tc, "Error allocating shared memory block", rv);
    ABTS_PTR_NOTNULL(tc, shm);

    boxes = fspr_shm_baseaddr_get(shm);
    ABTS_PTR_NOTNULL(tc, boxes);

    rv = fspr_shm_destroy(shm);
    APR_ASSERT_SUCCESS(tc, "Error destroying shared memory block", rv);
}

#if APR_HAS_FORK
static void test_anon(abts_case *tc, void *data)
{
    fspr_proc_t proc;
    fspr_status_t rv;
    fspr_shm_t *shm;
    fspr_size_t retsize;
    int cnt, i;
    int recvd;

    rv = fspr_shm_create(&shm, SHARED_SIZE, NULL, p);
    APR_ASSERT_SUCCESS(tc, "Error allocating shared memory block", rv);
    ABTS_PTR_NOTNULL(tc, shm);

    retsize = fspr_shm_size_get(shm);
    ABTS_INT_EQUAL(tc, SHARED_SIZE, retsize);

    boxes = fspr_shm_baseaddr_get(shm);
    ABTS_PTR_NOTNULL(tc, boxes);

    rv = fspr_proc_fork(&proc, p);
    if (rv == APR_INCHILD) { /* child */
        int num = msgwait(5, 0, N_BOXES);
        /* exit with the number of messages received so that the parent
         * can check that all messages were received.
         */
        exit(num);
    }
    else if (rv == APR_INPARENT) { /* parent */
        i = N_BOXES;
        cnt = 0;
        while (cnt++ < N_MESSAGES) {
            if ((i-=3) < 0) {
                i += N_BOXES; /* start over at the top */
            }
            msgput(i, MSG);
            fspr_sleep(fspr_time_make(0, 10000));
        }
    }
    else {
        ABTS_FAIL(tc, "fspr_proc_fork failed");
    }
    /* wait for the child */
    rv = fspr_proc_wait(&proc, &recvd, NULL, APR_WAIT);
    ABTS_INT_EQUAL(tc, N_MESSAGES, recvd);

    rv = fspr_shm_destroy(shm);
    APR_ASSERT_SUCCESS(tc, "Error destroying shared memory block", rv);
}
#endif

static void test_named(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_shm_t *shm = NULL;
    fspr_size_t retsize;
    fspr_proc_t pidproducer, pidconsumer;
    fspr_procattr_t *attr1 = NULL, *attr2 = NULL;
    int sent, received;
    fspr_exit_why_e why;
    const char *args[4];

    fspr_shm_remove(SHARED_FILENAME, p);

    rv = fspr_shm_create(&shm, SHARED_SIZE, SHARED_FILENAME, p);
    APR_ASSERT_SUCCESS(tc, "Error allocating shared memory block", rv);
    if (rv != APR_SUCCESS) {
        return;
    }
    ABTS_PTR_NOTNULL(tc, shm);

    retsize = fspr_shm_size_get(shm);
    ABTS_INT_EQUAL(tc, SHARED_SIZE, retsize);

    boxes = fspr_shm_baseaddr_get(shm);
    ABTS_PTR_NOTNULL(tc, boxes);

    rv = fspr_procattr_create(&attr1, p);
    ABTS_PTR_NOTNULL(tc, attr1);
    APR_ASSERT_SUCCESS(tc, "Couldn't create attr1", rv);
    args[0] = fspr_pstrdup(p, "testshmproducer" EXTENSION);
    args[1] = NULL;
    rv = fspr_proc_create(&pidproducer, "./testshmproducer" EXTENSION, args,
                         NULL, attr1, p);
    APR_ASSERT_SUCCESS(tc, "Couldn't launch producer", rv);

    rv = fspr_procattr_create(&attr2, p);
    ABTS_PTR_NOTNULL(tc, attr2);
    APR_ASSERT_SUCCESS(tc, "Couldn't create attr2", rv);
    args[0] = fspr_pstrdup(p, "testshmconsumer" EXTENSION);
    rv = fspr_proc_create(&pidconsumer, "./testshmconsumer" EXTENSION, args, 
                         NULL, attr2, p);
    APR_ASSERT_SUCCESS(tc, "Couldn't launch consumer", rv);

    rv = fspr_proc_wait(&pidconsumer, &received, &why, APR_WAIT);
    ABTS_INT_EQUAL(tc, APR_CHILD_DONE, rv);
    ABTS_INT_EQUAL(tc, APR_PROC_EXIT, why);

    rv = fspr_proc_wait(&pidproducer, &sent, &why, APR_WAIT);
    ABTS_INT_EQUAL(tc, APR_CHILD_DONE, rv);
    ABTS_INT_EQUAL(tc, APR_PROC_EXIT, why);

    /* Cleanup before testing that producer and consumer worked correctly.
     * This way, if they didn't succeed, we can just run this test again
     * without having to cleanup manually.
     */
    APR_ASSERT_SUCCESS(tc, "Error destroying shared memory", 
                       fspr_shm_destroy(shm));
    
    ABTS_INT_EQUAL(tc, sent, received);

}

static void test_named_remove(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_shm_t *shm;

    fspr_shm_remove(SHARED_FILENAME, p);

    rv = fspr_shm_create(&shm, SHARED_SIZE, SHARED_FILENAME, p);
    APR_ASSERT_SUCCESS(tc, "Error allocating shared memory block", rv);
    if (rv != APR_SUCCESS) {
        return;
    }
    ABTS_PTR_NOTNULL(tc, shm);

    rv = fspr_shm_remove(SHARED_FILENAME, p);
    APR_ASSERT_SUCCESS(tc, "Error removing shared memory block", rv);
    if (rv != APR_SUCCESS) {
        return ;
    }

    rv = fspr_shm_create(&shm, SHARED_SIZE, SHARED_FILENAME, p);
    APR_ASSERT_SUCCESS(tc, "Error allocating shared memory block", rv);
    if (rv != APR_SUCCESS) {
        return;
    }
    ABTS_PTR_NOTNULL(tc, shm);

    rv = fspr_shm_destroy(shm);
    APR_ASSERT_SUCCESS(tc, "Error destroying shared memory block", rv);
}

#endif

abts_suite *testshm(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

#if APR_HAS_SHARED_MEMORY
    abts_run_test(suite, test_anon_create, NULL);
    abts_run_test(suite, test_check_size, NULL);
    abts_run_test(suite, test_shm_allocate, NULL);
#if APR_HAS_FORK
    abts_run_test(suite, test_anon, NULL);
#endif
    abts_run_test(suite, test_named, NULL); 
    abts_run_test(suite, test_named_remove, NULL); 
#endif

    return suite;
}


