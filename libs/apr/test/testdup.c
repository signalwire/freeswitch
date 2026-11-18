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


#include "fspr_general.h"
#include "fspr_pools.h"
#include "fspr_errno.h"
#include "fspr_file_io.h"
#include "testutil.h"

#define TEST "Testing\n"
#define TEST2 "Testing again\n"
#define FILEPATH "data/"

static void test_file_dup(abts_case *tc, void *data)
{
    fspr_file_t *file1 = NULL;
    fspr_file_t *file3 = NULL;
    fspr_status_t rv;
    fspr_finfo_t finfo;

    /* First, create a new file, empty... */
    rv = fspr_file_open(&file1, FILEPATH "testdup.file", 
                       APR_READ | APR_WRITE | APR_CREATE |
                       APR_DELONCLOSE, APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, file1);

    rv = fspr_file_dup(&file3, file1, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, file3);

    rv = fspr_file_close(file1);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    /* cleanup after ourselves */
    rv = fspr_file_close(file3);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_stat(&finfo, FILEPATH "testdup.file", APR_FINFO_NORM, p);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_ENOENT(rv));
}  

static void test_file_readwrite(abts_case *tc, void *data)
{
    fspr_file_t *file1 = NULL;
    fspr_file_t *file3 = NULL;
    fspr_status_t rv;
    fspr_finfo_t finfo;
    fspr_size_t txtlen = sizeof(TEST);
    char buff[50];
    fspr_off_t fpos;

    /* First, create a new file, empty... */
    rv = fspr_file_open(&file1, FILEPATH "testdup.readwrite.file", 
                       APR_READ | APR_WRITE | APR_CREATE |
                       APR_DELONCLOSE, APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, file1);

    rv = fspr_file_dup(&file3, file1, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, file3);

    rv = fspr_file_write(file3, TEST, &txtlen);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, sizeof(TEST), txtlen);

    fpos = 0;
    rv = fspr_file_seek(file1, APR_SET, &fpos);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_ASSERT(tc, "File position mismatch, expected 0", fpos == 0);

    txtlen = 50;
    rv = fspr_file_read(file1, buff, &txtlen);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_STR_EQUAL(tc, TEST, buff);

    /* cleanup after ourselves */
    rv = fspr_file_close(file1);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_file_close(file3);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_stat(&finfo, FILEPATH "testdup.readwrite.file", APR_FINFO_NORM, p);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_ENOENT(rv));
}  

static void test_dup2(abts_case *tc, void *data)
{
    fspr_file_t *testfile = NULL;
    fspr_file_t *errfile = NULL;
    fspr_file_t *saveerr = NULL;
    fspr_status_t rv;

    rv = fspr_file_open(&testfile, FILEPATH "testdup2.file", 
                       APR_READ | APR_WRITE | APR_CREATE |
                       APR_DELONCLOSE, APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, testfile);

    rv = fspr_file_open_stderr(&errfile, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    /* Set aside the real errfile */
    rv = fspr_file_dup(&saveerr, errfile, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, saveerr);

    rv = fspr_file_dup2(errfile, testfile, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, errfile);

    fspr_file_close(testfile);

    rv = fspr_file_dup2(errfile, saveerr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, errfile);
}

static void test_dup2_readwrite(abts_case *tc, void *data)
{
    fspr_file_t *errfile = NULL;
    fspr_file_t *testfile = NULL;
    fspr_file_t *saveerr = NULL;
    fspr_status_t rv;
    fspr_size_t txtlen = sizeof(TEST);
    char buff[50];
    fspr_off_t fpos;

    rv = fspr_file_open(&testfile, FILEPATH "testdup2.readwrite.file", 
                       APR_READ | APR_WRITE | APR_CREATE |
                       APR_DELONCLOSE, APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, testfile);

    rv = fspr_file_open_stderr(&errfile, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    /* Set aside the real errfile */
    rv = fspr_file_dup(&saveerr, errfile, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, saveerr);

    rv = fspr_file_dup2(errfile, testfile, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, errfile);

    txtlen = sizeof(TEST2);
    rv = fspr_file_write(errfile, TEST2, &txtlen);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, sizeof(TEST2), txtlen);

    fpos = 0;
    rv = fspr_file_seek(testfile, APR_SET, &fpos);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_ASSERT(tc, "File position mismatch, expected 0", fpos == 0);

    txtlen = 50;
    rv = fspr_file_read(testfile, buff, &txtlen);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_STR_EQUAL(tc, TEST2, buff);
      
    fspr_file_close(testfile);

    rv = fspr_file_dup2(errfile, saveerr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, errfile);
}

abts_suite *testdup(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, test_file_dup, NULL);
    abts_run_test(suite, test_file_readwrite, NULL);
    abts_run_test(suite, test_dup2, NULL);
    abts_run_test(suite, test_dup2_readwrite, NULL);

    return suite;
}

