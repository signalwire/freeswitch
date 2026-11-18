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
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "testutil.h"

#define TESTSTR "This is a test"

static fspr_proc_t newproc;

static void test_create_proc(abts_case *tc, void *data)
{
    const char *args[2];
    fspr_procattr_t *attr;
    fspr_file_t *testfile = NULL;
    fspr_status_t rv;
    fspr_size_t length;
    char *buf;

    rv = fspr_procattr_create(&attr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_procattr_io_set(attr, APR_FULL_BLOCK, APR_FULL_BLOCK, 
                             APR_NO_PIPE);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_procattr_dir_set(attr, "data");
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_procattr_cmdtype_set(attr, APR_PROGRAM);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    args[0] = "proc_child" EXTENSION;
    args[1] = NULL;
    
    rv = fspr_proc_create(&newproc, "../proc_child" EXTENSION, args, NULL, 
                         attr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    testfile = newproc.in;

    length = strlen(TESTSTR);
    rv = fspr_file_write(testfile, TESTSTR, &length);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, strlen(TESTSTR), length);

    testfile = newproc.out;
    length = 256;
    buf = fspr_pcalloc(p, length);
    rv = fspr_file_read(testfile, buf, &length);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_STR_EQUAL(tc, TESTSTR, buf);
}

static void test_proc_wait(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_proc_wait(&newproc, NULL, NULL, APR_WAIT);
    ABTS_INT_EQUAL(tc, APR_CHILD_DONE, rv);
}

static void test_file_redir(abts_case *tc, void *data)
{
    fspr_file_t *testout = NULL;
    fspr_file_t *testerr = NULL;
    fspr_off_t offset;
    fspr_status_t rv;
    const char *args[2];
    fspr_procattr_t *attr;
    fspr_file_t *testfile = NULL;
    fspr_size_t length;
    char *buf;

    testfile = NULL;
    rv = fspr_file_open(&testfile, "data/stdin",
                       APR_READ | APR_WRITE | APR_CREATE | APR_EXCL,
                       APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_file_open(&testout, "data/stdout",
                       APR_READ | APR_WRITE | APR_CREATE | APR_EXCL,
                       APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_file_open(&testerr, "data/stderr",
                       APR_READ | APR_WRITE | APR_CREATE | APR_EXCL,
                       APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    length = strlen(TESTSTR);
    fspr_file_write(testfile, TESTSTR, &length);
    offset = 0;
    rv = fspr_file_seek(testfile, APR_SET, &offset);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_ASSERT(tc, "File position mismatch, expected 0", offset == 0);

    rv = fspr_procattr_create(&attr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_procattr_child_in_set(attr, testfile, NULL);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_procattr_child_out_set(attr, testout, NULL);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_procattr_child_err_set(attr, testerr, NULL);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_procattr_dir_set(attr, "data");
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_procattr_cmdtype_set(attr, APR_PROGRAM);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    args[0] = "proc_child";
    args[1] = NULL;

    rv = fspr_proc_create(&newproc, "../proc_child" EXTENSION, args, NULL, 
                         attr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_proc_wait(&newproc, NULL, NULL, APR_WAIT);
    ABTS_INT_EQUAL(tc, APR_CHILD_DONE, rv);

    offset = 0;
    rv = fspr_file_seek(testout, APR_SET, &offset);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    length = 256;
    buf = fspr_pcalloc(p, length);
    rv = fspr_file_read(testout, buf, &length);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_STR_EQUAL(tc, TESTSTR, buf);


    fspr_file_close(testfile);
    fspr_file_close(testout);
    fspr_file_close(testerr);

    rv = fspr_file_remove("data/stdin", p);;
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_file_remove("data/stdout", p);;
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_file_remove("data/stderr", p);;
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
}

abts_suite *testproc(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, test_create_proc, NULL);
    abts_run_test(suite, test_proc_wait, NULL);
    abts_run_test(suite, test_file_redir, NULL);

    return suite;
}

