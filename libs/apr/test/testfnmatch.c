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
#include "apr_file_info.h"
#include "apr_fnmatch.h"
#include "apr_tables.h"

/* XXX NUM_FILES must be equal to the nummber of expected files with a
 * .txt extension in the data directory at the time testfnmatch
 * happens to be run (!?!). */

#define NUM_FILES (5)

static void test_glob(abts_case *tc, void *data)
{
    int i;
    char **list;
    apr_array_header_t *result;
    
    APR_ASSERT_SUCCESS(tc, "glob match against data/*.txt",
                       apr_match_glob("data\\*.txt", &result, p));

    ABTS_INT_EQUAL(tc, NUM_FILES, result->nelts);

    list = (char **)result->elts;
    for (i = 0; i < result->nelts; i++) {
        char *dot = strrchr(list[i], '.');
        ABTS_STR_EQUAL(tc, dot, ".txt");
    }
}

static void test_glob_currdir(abts_case *tc, void *data)
{
    int i;
    char **list;
    apr_array_header_t *result;
    apr_filepath_set("data", p);
    
    APR_ASSERT_SUCCESS(tc, "glob match against *.txt with data as current",
                       apr_match_glob("*.txt", &result, p));


    ABTS_INT_EQUAL(tc, NUM_FILES, result->nelts);

    list = (char **)result->elts;
    for (i = 0; i < result->nelts; i++) {
        char *dot = strrchr(list[i], '.');
        ABTS_STR_EQUAL(tc, dot, ".txt");
    }
    apr_filepath_set("..", p);
}

abts_suite *testfnmatch(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, test_glob, NULL);
    abts_run_test(suite, test_glob_currdir, NULL);

    return suite;
}

