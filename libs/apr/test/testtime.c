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

#include "fspr_time.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "testutil.h"
#include "fspr_strings.h"
#include <time.h>

#define STR_SIZE 45

/* The time value is used throughout the tests, so just make this a global.
 * Also, we need a single value that we can test for the positive tests, so
 * I chose the number below, it corresponds to:
 *           2002-08-14 12:05:36.186711 -25200 [257 Sat].
 * Which happens to be when I wrote the new tests.
 */
static fspr_time_t now = APR_INT64_C(1032030336186711);

static char* print_time (fspr_pool_t *pool, const fspr_time_exp_t *xt)
{
    return fspr_psprintf (pool,
                         "%04d-%02d-%02d %02d:%02d:%02d.%06d %+05d [%d %s]%s",
                         xt->tm_year + 1900,
                         xt->tm_mon,
                         xt->tm_mday,
                         xt->tm_hour,
                         xt->tm_min,
                         xt->tm_sec,
                         xt->tm_usec,
                         xt->tm_gmtoff,
                         xt->tm_yday + 1,
                         fspr_day_snames[xt->tm_wday],
                         (xt->tm_isdst ? " DST" : ""));
}


static void test_now(abts_case *tc, void *data)
{
    fspr_time_t timediff;
    fspr_time_t current;
    time_t os_now;

    current = fspr_time_now();
    time(&os_now);

    timediff = os_now - (current / APR_USEC_PER_SEC); 
    /* Even though these are called so close together, there is the chance
     * that the time will be slightly off, so accept anything between -1 and
     * 1 second.
     */
    ABTS_ASSERT(tc, "fspr_time and OS time do not agree", 
             (timediff > -2) && (timediff < 2));
}

static void test_gmtstr(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;

    rv = fspr_time_exp_gmt(&xt, now);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_time_exp_gmt");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    ABTS_STR_EQUAL(tc, "2002-08-14 19:05:36.186711 +0000 [257 Sat]", 
                      print_time(p, &xt));
}

static void test_exp_lt(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;
    time_t posix_secs = (time_t)fspr_time_sec(now);
    struct tm *posix_exp = localtime(&posix_secs);

    rv = fspr_time_exp_lt(&xt, now);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_time_exp_lt");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);

#define CHK_FIELD(f) \
    ABTS_ASSERT(tc, "Mismatch in " #f, posix_exp->f == xt.f)

    CHK_FIELD(tm_sec);
    CHK_FIELD(tm_min);
    CHK_FIELD(tm_hour);
    CHK_FIELD(tm_mday);
    CHK_FIELD(tm_mon);
    CHK_FIELD(tm_year);
    CHK_FIELD(tm_wday);
    CHK_FIELD(tm_yday);
    CHK_FIELD(tm_isdst);
#undef CHK_FIELD
}

static void test_exp_get_gmt(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;
    fspr_time_t imp;
    fspr_int64_t hr_off_64;

    rv = fspr_time_exp_gmt(&xt, now);
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    rv = fspr_time_exp_get(&imp, &xt);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_time_exp_get");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    hr_off_64 = (fspr_int64_t) xt.tm_gmtoff * APR_USEC_PER_SEC;
    ABTS_TRUE(tc, now + hr_off_64 == imp);
}

static void test_exp_get_lt(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;
    fspr_time_t imp;
    fspr_int64_t hr_off_64;

    rv = fspr_time_exp_lt(&xt, now);
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    rv = fspr_time_exp_get(&imp, &xt);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_time_exp_get");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    hr_off_64 = (fspr_int64_t) xt.tm_gmtoff * APR_USEC_PER_SEC;
    ABTS_TRUE(tc, now + hr_off_64 == imp);
}

static void test_imp_gmt(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;
    fspr_time_t imp;

    rv = fspr_time_exp_gmt(&xt, now);
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    rv = fspr_time_exp_gmt_get(&imp, &xt);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_time_exp_gmt_get");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    ABTS_TRUE(tc, now == imp);
}

static void test_rfcstr(abts_case *tc, void *data)
{
    fspr_status_t rv;
    char str[STR_SIZE];

    rv = fspr_rfc822_date(str, now);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_rfc822_date");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    ABTS_STR_EQUAL(tc, "Sat, 14 Sep 2002 19:05:36 GMT", str);
}

static void test_ctime(abts_case *tc, void *data)
{
    fspr_status_t rv;
    char fspr_str[STR_SIZE];
    char libc_str[STR_SIZE];
    fspr_time_t now_sec = fspr_time_sec(now);
    time_t posix_sec = (time_t) now_sec;

    rv = fspr_ctime(fspr_str, now);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_ctime");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    strcpy(libc_str, ctime(&posix_sec));
    *strchr(libc_str, '\n') = '\0';

    ABTS_STR_EQUAL(tc, libc_str, fspr_str);
}

static void test_strftime(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;
    char *str = NULL;
    fspr_size_t sz;

    rv = fspr_time_exp_gmt(&xt, now);
    str = fspr_palloc(p, STR_SIZE + 1);
    rv = fspr_strftime(str, &sz, STR_SIZE, "%R %A %d %B %Y", &xt);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_strftime");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    ABTS_STR_EQUAL(tc, "19:05 Saturday 14 September 2002", str);
}

static void test_strftimesmall(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;
    char str[STR_SIZE];
    fspr_size_t sz;

    rv = fspr_time_exp_gmt(&xt, now);
    rv = fspr_strftime(str, &sz, STR_SIZE, "%T", &xt);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_strftime");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    ABTS_STR_EQUAL(tc, "19:05:36", str);
}

static void test_exp_tz(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;
    fspr_int32_t hr_off = -5 * 3600; /* 5 hours in seconds */

    rv = fspr_time_exp_tz(&xt, now, hr_off);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_time_exp_tz");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
    ABTS_TRUE(tc, (xt.tm_usec == 186711) && 
                     (xt.tm_sec == 36) &&
                     (xt.tm_min == 5) && 
                     (xt.tm_hour == 14) &&
                     (xt.tm_mday == 14) &&
                     (xt.tm_mon == 8) &&
                     (xt.tm_year == 102) &&
                     (xt.tm_wday == 6) &&
                     (xt.tm_yday == 256));
}

static void test_strftimeoffset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_time_exp_t xt;
    char str[STR_SIZE];
    fspr_size_t sz;
    fspr_int32_t hr_off = -5 * 3600; /* 5 hours in seconds */

    fspr_time_exp_tz(&xt, now, hr_off);
    rv = fspr_strftime(str, &sz, STR_SIZE, "%T", &xt);
    if (rv == APR_ENOTIMPL) {
        ABTS_NOT_IMPL(tc, "fspr_strftime");
    }
    ABTS_TRUE(tc, rv == APR_SUCCESS);
}

/* 0.9.4 and earlier rejected valid dates in 2038 */
static void test_2038(abts_case *tc, void *data)
{
    fspr_time_exp_t xt;
    fspr_time_t t;

    /* 2038-01-19T03:14:07.000000Z */
    xt.tm_year = 138;
    xt.tm_mon = 0;
    xt.tm_mday = 19;
    xt.tm_hour = 3;
    xt.tm_min = 14;
    xt.tm_sec = 7;
    
    APR_ASSERT_SUCCESS(tc, "explode January 19th, 2038",
                       fspr_time_exp_get(&t, &xt));
}

abts_suite *testtime(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, test_now, NULL);
    abts_run_test(suite, test_gmtstr, NULL);
    abts_run_test(suite, test_exp_lt, NULL);
    abts_run_test(suite, test_exp_get_gmt, NULL);
    abts_run_test(suite, test_exp_get_lt, NULL);
    abts_run_test(suite, test_imp_gmt, NULL);
    abts_run_test(suite, test_rfcstr, NULL);
    abts_run_test(suite, test_ctime, NULL);
    abts_run_test(suite, test_strftime, NULL);
    abts_run_test(suite, test_strftimesmall, NULL);
    abts_run_test(suite, test_exp_tz, NULL);
    abts_run_test(suite, test_strftimeoffset, NULL);
    abts_run_test(suite, test_2038, NULL);

    return suite;
}

