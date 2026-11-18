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

#include <stdio.h>
#include <stdlib.h>
#include "fspr_file_io.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "testutil.h"

static fspr_pool_t *pool;
static char *testdata;
static int cleanup_called = 0;

static fspr_status_t string_cleanup(void *data)
{
    cleanup_called = 1;
    return APR_SUCCESS;
}

static void set_userdata(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_pool_userdata_set(testdata, "TEST", string_cleanup, pool);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
}

static void get_userdata(abts_case *tc, void *data)
{
    fspr_status_t rv;
    void *retdata;

    rv = fspr_pool_userdata_get(&retdata, "TEST", pool);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
    ABTS_STR_EQUAL(tc, retdata, testdata);
}

static void get_nonexistkey(abts_case *tc, void *data)
{
    fspr_status_t rv;
    void *retdata;

    rv = fspr_pool_userdata_get(&retdata, "DOESNTEXIST", pool);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
    ABTS_PTR_EQUAL(tc, retdata, NULL);
}

static void post_pool_clear(abts_case *tc, void *data)
{
    fspr_status_t rv;
    void *retdata;

    rv = fspr_pool_userdata_get(&retdata, "DOESNTEXIST", pool);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
    ABTS_PTR_EQUAL(tc, retdata, NULL);
}

abts_suite *testud(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    fspr_pool_create(&pool, p);
    testdata = fspr_pstrdup(pool, "This is a test\n");

    abts_run_test(suite, set_userdata, NULL);
    abts_run_test(suite, get_userdata, NULL);
    abts_run_test(suite, get_nonexistkey, NULL);

    fspr_pool_clear(pool);

    abts_run_test(suite, post_pool_clear, NULL);

    return suite;
}

