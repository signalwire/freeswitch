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
#include "fspr.h"
#include "fspr_strings.h"
#include "fspr_general.h"
#include "fspr_pools.h"
#include "fspr_tables.h"
#if APR_HAVE_STDIO_H
#include <stdio.h>
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if APR_HAVE_STRING_H
#include <string.h>
#endif

static fspr_table_t *t1 = NULL;

static void table_make(abts_case *tc, void *data)
{
    t1 = fspr_table_make(p, 5);
    ABTS_PTR_NOTNULL(tc, t1);
}

static void table_get(abts_case *tc, void *data)
{
    const char *val;

    fspr_table_set(t1, "foo", "bar");
    val = fspr_table_get(t1, "foo");
    ABTS_STR_EQUAL(tc, val, "bar");
}

static void table_set(abts_case *tc, void *data)
{
    const char *val;

    fspr_table_set(t1, "setkey", "bar");
    fspr_table_set(t1, "setkey", "2ndtry");
    val = fspr_table_get(t1, "setkey");
    ABTS_STR_EQUAL(tc, val, "2ndtry");
}

static void table_getnotthere(abts_case *tc, void *data)
{
    const char *val;

    val = fspr_table_get(t1, "keynotthere");
    ABTS_PTR_EQUAL(tc, NULL, (void *)val);
}

static void table_add(abts_case *tc, void *data)
{
    const char *val;

    fspr_table_add(t1, "addkey", "bar");
    fspr_table_add(t1, "addkey", "foo");
    val = fspr_table_get(t1, "addkey");
    ABTS_STR_EQUAL(tc, val, "bar");

}

static void table_nelts(abts_case *tc, void *data)
{
    const char *val;
    fspr_table_t *t = fspr_table_make(p, 1);

    fspr_table_set(t, "abc", "def");
    fspr_table_set(t, "def", "abc");
    fspr_table_set(t, "foo", "zzz");
    val = fspr_table_get(t, "foo");
    ABTS_STR_EQUAL(tc, val, "zzz");
    val = fspr_table_get(t, "abc");
    ABTS_STR_EQUAL(tc, val, "def");
    val = fspr_table_get(t, "def");
    ABTS_STR_EQUAL(tc, val, "abc");
    ABTS_INT_EQUAL(tc, 3, fspr_table_elts(t)->nelts);
}

static void table_clear(abts_case *tc, void *data)
{
    fspr_table_clear(t1);
    ABTS_INT_EQUAL(tc, 0, fspr_table_elts(t1)->nelts);
}

static void table_unset(abts_case *tc, void *data)
{
    const char *val;
    fspr_table_t *t = fspr_table_make(p, 1);

    fspr_table_set(t, "a", "1");
    fspr_table_set(t, "b", "2");
    fspr_table_unset(t, "b");
    ABTS_INT_EQUAL(tc, 1, fspr_table_elts(t)->nelts);
    val = fspr_table_get(t, "a");
    ABTS_STR_EQUAL(tc, val, "1");
    val = fspr_table_get(t, "b");
    ABTS_PTR_EQUAL(tc, (void *)val, (void *)NULL);
}

static void table_overlap(abts_case *tc, void *data)
{
    const char *val;
    fspr_table_t *t1 = fspr_table_make(p, 1);
    fspr_table_t *t2 = fspr_table_make(p, 1);

    fspr_table_addn(t1, "a", "0");
    fspr_table_addn(t1, "g", "7");
    fspr_table_addn(t2, "a", "1");
    fspr_table_addn(t2, "b", "2");
    fspr_table_addn(t2, "c", "3");
    fspr_table_addn(t2, "b", "2.0");
    fspr_table_addn(t2, "d", "4");
    fspr_table_addn(t2, "e", "5");
    fspr_table_addn(t2, "b", "2.");
    fspr_table_addn(t2, "f", "6");
    fspr_table_overlap(t1, t2, APR_OVERLAP_TABLES_SET);
    
    ABTS_INT_EQUAL(tc, fspr_table_elts(t1)->nelts, 7);
    val = fspr_table_get(t1, "a");
    ABTS_STR_EQUAL(tc, val, "1");
    val = fspr_table_get(t1, "b");
    ABTS_STR_EQUAL(tc, val, "2.");
    val = fspr_table_get(t1, "c");
    ABTS_STR_EQUAL(tc, val, "3");
    val = fspr_table_get(t1, "d");
    ABTS_STR_EQUAL(tc, val, "4");
    val = fspr_table_get(t1, "e");
    ABTS_STR_EQUAL(tc, val, "5");
    val = fspr_table_get(t1, "f");
    ABTS_STR_EQUAL(tc, val, "6");
    val = fspr_table_get(t1, "g");
    ABTS_STR_EQUAL(tc, val, "7");
}

static void table_overlap2(abts_case *tc, void *data)
{
    fspr_pool_t *subp;
    fspr_table_t *t1, *t2;

    fspr_pool_create(&subp, p);

    t1 = fspr_table_make(subp, 1);
    t2 = fspr_table_make(p, 1);
    fspr_table_addn(t1, "t1", "one");
    fspr_table_addn(t2, "t2", "two");
    
    fspr_table_overlap(t1, t2, APR_OVERLAP_TABLES_SET);
    
    ABTS_INT_EQUAL(tc, 2, fspr_table_elts(t1)->nelts);
    
    ABTS_STR_EQUAL(tc, fspr_table_get(t1, "t1"), "one");
    ABTS_STR_EQUAL(tc, fspr_table_get(t1, "t2"), "two");

}

abts_suite *testtable(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, table_make, NULL);
    abts_run_test(suite, table_get, NULL);
    abts_run_test(suite, table_set, NULL);
    abts_run_test(suite, table_getnotthere, NULL);
    abts_run_test(suite, table_add, NULL);
    abts_run_test(suite, table_nelts, NULL);
    abts_run_test(suite, table_clear, NULL);
    abts_run_test(suite, table_unset, NULL);
    abts_run_test(suite, table_overlap, NULL);
    abts_run_test(suite, table_overlap2, NULL);

    return suite;
}

