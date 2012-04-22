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

#include "apr_file_io.h"
#include "apr_file_info.h"
#include "apr_errno.h"
#include "apr_general.h"
#include "apr_poll.h"
#include "apr_strings.h"
#include "apr_lib.h"
#include "apr_mmap.h"
#include "testutil.h"

/* Only enable these tests by default on platforms which support sparse
 * files... just Unixes? */
#if defined(WIN32) || defined(OS2) || defined(NETWARE)
static void test_nolfs(abts_case *tc, void *data)
{
    ABTS_NOT_IMPL(tc, "Large Files tests require Sparse file support");
}
#elif APR_HAS_LARGE_FILES
#define USE_LFS_TESTS

/* Tests which create an 8Gb sparse file and then check it can be used
 * as normal. */

static apr_off_t eightGb = APR_INT64_C(2) << 32;

static int madefile = 0;

#define PRECOND if (!madefile) { ABTS_NOT_IMPL(tc, "Large file tests not enabled"); return; }

#define TESTDIR "lfstests"
#define TESTFILE "large.bin"
#define TESTFN "lfstests/large.bin"

static void test_open(abts_case *tc, void *data)
{
    apr_file_t *f;
    apr_status_t rv;

    rv = apr_dir_make(TESTDIR, APR_OS_DEFAULT, p);
    if (rv && !APR_STATUS_IS_EEXIST(rv)) {
        APR_ASSERT_SUCCESS(tc, "make test directory", rv);
    }

    APR_ASSERT_SUCCESS(tc, "open file",
                       apr_file_open(&f, TESTFN, 
                                     APR_CREATE | APR_WRITE | APR_TRUNCATE,
                                     APR_OS_DEFAULT, p));

    rv = apr_file_trunc(f, eightGb);

    APR_ASSERT_SUCCESS(tc, "close large file", apr_file_close(f));

    /* 8Gb may pass rlimits or filesystem limits */

    if (APR_STATUS_IS_EINVAL(rv)
#ifdef EFBIG
        || rv == EFBIG
#endif
        ) {
        ABTS_NOT_IMPL(tc, "Creation of large file (limited by rlimit or fs?)");
    } 
    else {
        APR_ASSERT_SUCCESS(tc, "truncate file to 8gb", rv);
    }

    madefile = rv == APR_SUCCESS;
}

static void test_reopen(abts_case *tc, void *data)
{
    apr_file_t *fh;
    apr_finfo_t finfo;

    PRECOND;
    
    APR_ASSERT_SUCCESS(tc, "re-open 8Gb file",
                       apr_file_open(&fh, TESTFN, APR_READ, APR_OS_DEFAULT, p));

    APR_ASSERT_SUCCESS(tc, "file_info_get failed",
                       apr_file_info_get(&finfo, APR_FINFO_NORM, fh));
    
    ABTS_ASSERT(tc, "file_info_get gave incorrect size",
             finfo.size == eightGb);

    APR_ASSERT_SUCCESS(tc, "re-close large file", apr_file_close(fh));
}

static void test_stat(abts_case *tc, void *data)
{
    apr_finfo_t finfo;

    PRECOND;

    APR_ASSERT_SUCCESS(tc, "stat large file", 
                       apr_stat(&finfo, TESTFN, APR_FINFO_NORM, p));
    
    ABTS_ASSERT(tc, "stat gave incorrect size", finfo.size == eightGb);
}

static void test_readdir(abts_case *tc, void *data)
{
    apr_dir_t *dh;
    apr_status_t rv;

    PRECOND;

    APR_ASSERT_SUCCESS(tc, "open test directory", 
                       apr_dir_open(&dh, TESTDIR, p));

    do {
        apr_finfo_t finfo;
        
        rv = apr_dir_read(&finfo, APR_FINFO_NORM, dh);
        
        if (rv == APR_SUCCESS && strcmp(finfo.name, TESTFILE) == 0) {
            ABTS_ASSERT(tc, "apr_dir_read gave incorrect size for large file", 
                     finfo.size == eightGb);
        }

    } while (rv == APR_SUCCESS);
        
    if (!APR_STATUS_IS_ENOENT(rv)) {
        APR_ASSERT_SUCCESS(tc, "apr_dir_read failed", rv);
    }
    
    APR_ASSERT_SUCCESS(tc, "close test directory",
                       apr_dir_close(dh));
}

#define TESTSTR "Hello, world."

static void test_append(abts_case *tc, void *data)
{
    apr_file_t *fh;
    apr_finfo_t finfo;
    
    PRECOND;

    APR_ASSERT_SUCCESS(tc, "open 8Gb file for append",
                       apr_file_open(&fh, TESTFN, APR_WRITE | APR_APPEND, 
                                     APR_OS_DEFAULT, p));

    APR_ASSERT_SUCCESS(tc, "append to 8Gb file",
                       apr_file_write_full(fh, TESTSTR, strlen(TESTSTR), NULL));

    APR_ASSERT_SUCCESS(tc, "file_info_get failed",
                       apr_file_info_get(&finfo, APR_FINFO_NORM, fh));
    
    ABTS_ASSERT(tc, "file_info_get gave incorrect size",
             finfo.size == eightGb + strlen(TESTSTR));

    APR_ASSERT_SUCCESS(tc, "close 8Gb file", apr_file_close(fh));
}

static void test_seek(abts_case *tc, void *data)
{
    apr_file_t *fh;
    apr_off_t pos;

    PRECOND;
    
    APR_ASSERT_SUCCESS(tc, "open 8Gb file for writing",
                       apr_file_open(&fh, TESTFN, APR_WRITE, 
                                     APR_OS_DEFAULT, p));

    pos = 0;
    APR_ASSERT_SUCCESS(tc, "relative seek to end", 
                       apr_file_seek(fh, APR_END, &pos));
    ABTS_ASSERT(tc, "seek to END gave 8Gb", pos == eightGb);
    
    pos = eightGb;
    APR_ASSERT_SUCCESS(tc, "seek to 8Gb", apr_file_seek(fh, APR_SET, &pos));
    ABTS_ASSERT(tc, "seek gave 8Gb offset", pos == eightGb);

    pos = 0;
    APR_ASSERT_SUCCESS(tc, "relative seek to 0", apr_file_seek(fh, APR_CUR, &pos));
    ABTS_ASSERT(tc, "relative seek gave 8Gb offset", pos == eightGb);

    apr_file_close(fh);
}

static void test_write(abts_case *tc, void *data)
{
    apr_file_t *fh;
    apr_off_t pos = eightGb - 4;

    PRECOND;

    APR_ASSERT_SUCCESS(tc, "re-open 8Gb file",
                       apr_file_open(&fh, TESTFN, APR_WRITE, APR_OS_DEFAULT, p));

    APR_ASSERT_SUCCESS(tc, "seek to 8Gb - 4", 
                       apr_file_seek(fh, APR_SET, &pos));
    ABTS_ASSERT(tc, "seek gave 8Gb-4 offset", pos == eightGb - 4);

    APR_ASSERT_SUCCESS(tc, "write magic string to 8Gb-4",
                       apr_file_write_full(fh, "FISH", 4, NULL));

    APR_ASSERT_SUCCESS(tc, "close 8Gb file", apr_file_close(fh));
}


#if APR_HAS_MMAP
static void test_mmap(abts_case *tc, void *data)
{
    apr_mmap_t *map;
    apr_file_t *fh;
    apr_size_t len = 16384; /* hopefully a multiple of the page size */
    apr_off_t off = eightGb - len; 
    void *ptr;

    PRECOND;

    APR_ASSERT_SUCCESS(tc, "open 8gb file for mmap",
                       apr_file_open(&fh, TESTFN, APR_READ, APR_OS_DEFAULT, p));
    
    APR_ASSERT_SUCCESS(tc, "mmap 8Gb file",
                       apr_mmap_create(&map, fh, off, len, APR_MMAP_READ, p));

    APR_ASSERT_SUCCESS(tc, "close file", apr_file_close(fh));

    ABTS_ASSERT(tc, "mapped a 16K block", map->size == len);
    
    APR_ASSERT_SUCCESS(tc, "get pointer into mmaped region",
                       apr_mmap_offset(&ptr, map, len - 4));
    ABTS_ASSERT(tc, "pointer was not NULL", ptr != NULL);

    ABTS_ASSERT(tc, "found the magic string", memcmp(ptr, "FISH", 4) == 0);

    APR_ASSERT_SUCCESS(tc, "delete mmap handle", apr_mmap_delete(map));
}
#endif /* APR_HAS_MMAP */

static void test_format(abts_case *tc, void *data)
{
    apr_off_t off;

    PRECOND;

    off = apr_atoi64(apr_off_t_toa(p, eightGb));

    ABTS_ASSERT(tc, "apr_atoi64 parsed apr_off_t_toa result incorrectly",
             off == eightGb);
}

#else
static void test_nolfs(abts_case *tc, void *data)
{
    ABTS_NOT_IMPL(tc, "Large Files not supported");
}
#endif

abts_suite *testlfs(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

#ifdef USE_LFS_TESTS
    abts_run_test(suite, test_open, NULL);
    abts_run_test(suite, test_reopen, NULL);
    abts_run_test(suite, test_stat, NULL);
    abts_run_test(suite, test_readdir, NULL);
    abts_run_test(suite, test_seek, NULL);
    abts_run_test(suite, test_append, NULL);
    abts_run_test(suite, test_write, NULL);
#if APR_HAS_MMAP
    abts_run_test(suite, test_mmap, NULL);
#endif
    abts_run_test(suite, test_format, NULL);
#else
    abts_run_test(suite, test_nolfs, NULL);
#endif

    return suite;
}

