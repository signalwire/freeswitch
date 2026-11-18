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

#include <stdlib.h>

#include "testutil.h"
#include "fspr_file_io.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_thread_proc.h"
#include "fspr_strings.h"

static fspr_file_t *readp = NULL;
static fspr_file_t *writep = NULL;

static void create_pipe(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_file_pipe_create(&readp, &writep, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, readp);
    ABTS_PTR_NOTNULL(tc, writep);
}   

static void close_pipe(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_size_t nbytes = 256;
    char buf[256];

    rv = fspr_file_close(readp);
    rv = fspr_file_close(writep);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_file_read(readp, buf, &nbytes);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_EBADF(rv));
}   

static void set_timeout(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_interval_time_t timeout;

    rv = fspr_file_pipe_create(&readp, &writep, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, readp);
    ABTS_PTR_NOTNULL(tc, writep);

    rv = fspr_file_pipe_timeout_get(readp, &timeout);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_ASSERT(tc, "Timeout mismatch, expected -1", timeout == -1);

    rv = fspr_file_pipe_timeout_set(readp, fspr_time_from_sec(1));
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_file_pipe_timeout_get(readp, &timeout);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_ASSERT(tc, "Timeout mismatch, expected 1 second", 
                       timeout == fspr_time_from_sec(1));
}

static void read_write(abts_case *tc, void *data)
{
    fspr_status_t rv;
    char *buf;
    fspr_size_t nbytes;
    
    nbytes = strlen("this is a test");
    buf = (char *)fspr_palloc(p, nbytes + 1);

    rv = fspr_file_pipe_create(&readp, &writep, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, readp);
    ABTS_PTR_NOTNULL(tc, writep);

    rv = fspr_file_pipe_timeout_set(readp, fspr_time_from_sec(1));
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    if (!rv) {
        rv = fspr_file_read(readp, buf, &nbytes);
        ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_TIMEUP(rv));
        ABTS_INT_EQUAL(tc, 0, nbytes);
    }
}

static void read_write_notimeout(abts_case *tc, void *data)
{
    fspr_status_t rv;
    char *buf = "this is a test";
    char *input;
    fspr_size_t nbytes;
    
    nbytes = strlen("this is a test");

    rv = fspr_file_pipe_create(&readp, &writep, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, readp);
    ABTS_PTR_NOTNULL(tc, writep);

    rv = fspr_file_write(writep, buf, &nbytes);
    ABTS_INT_EQUAL(tc, strlen("this is a test"), nbytes);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    nbytes = 256;
    input = fspr_pcalloc(p, nbytes + 1);
    rv = fspr_file_read(readp, input, &nbytes);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, strlen("this is a test"), nbytes);
    ABTS_STR_EQUAL(tc, "this is a test", input);
}

static void test_pipe_writefull(abts_case *tc, void *data)
{
    int iterations = 1000;
    int i;
    int bytes_per_iteration = 8000;
    char *buf = (char *)malloc(bytes_per_iteration);
    char responsebuf[128];
    fspr_size_t nbytes;
    int bytes_processed;
    fspr_proc_t proc = {0};
    fspr_procattr_t *procattr;
    const char *args[2];
    fspr_status_t rv;
    fspr_exit_why_e why;
    
    rv = fspr_procattr_create(&procattr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_procattr_io_set(procattr, APR_CHILD_BLOCK, APR_CHILD_BLOCK,
                             APR_CHILD_BLOCK);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_procattr_error_check_set(procattr, 1);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    args[0] = "readchild" EXTENSION;
    args[1] = NULL;
    rv = fspr_proc_create(&proc, "./readchild" EXTENSION, args, NULL, procattr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_file_pipe_timeout_set(proc.in, fspr_time_from_sec(10));
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_file_pipe_timeout_set(proc.out, fspr_time_from_sec(10));
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    i = iterations;
    do {
        rv = fspr_file_write_full(proc.in, buf, bytes_per_iteration, NULL);
        ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    } while (--i);

    free(buf);

    rv = fspr_file_close(proc.in);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    
    nbytes = sizeof(responsebuf);
    rv = fspr_file_read(proc.out, responsebuf, &nbytes);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    bytes_processed = (int)fspr_strtoi64(responsebuf, NULL, 10);
    ABTS_INT_EQUAL(tc, iterations * bytes_per_iteration, bytes_processed);

    ABTS_ASSERT(tc, "wait for child process",
             fspr_proc_wait(&proc, NULL, &why, APR_WAIT) == APR_CHILD_DONE);
    
    ABTS_ASSERT(tc, "child terminated normally", why == APR_PROC_EXIT);
}

abts_suite *testpipe(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, create_pipe, NULL);
    abts_run_test(suite, close_pipe, NULL);
    abts_run_test(suite, set_timeout, NULL);
    abts_run_test(suite, close_pipe, NULL);
    abts_run_test(suite, read_write, NULL);
    abts_run_test(suite, close_pipe, NULL);
    abts_run_test(suite, read_write_notimeout, NULL);
    abts_run_test(suite, test_pipe_writefull, NULL);
    abts_run_test(suite, close_pipe, NULL);

    return suite;
}

