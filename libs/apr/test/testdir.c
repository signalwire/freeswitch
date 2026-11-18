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
#include <string.h>
#include "fspr_file_io.h"
#include "fspr_file_info.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "testutil.h"

static void test_mkdir(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_finfo_t finfo;

    rv = fspr_dir_make("data/testdir", APR_UREAD | APR_UWRITE | APR_UEXECUTE, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_stat(&finfo, "data/testdir", APR_FINFO_TYPE, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, APR_DIR, finfo.filetype);
}

static void test_mkdir_recurs(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_finfo_t finfo;

    rv = fspr_dir_make_recursive("data/one/two/three", 
                                APR_UREAD | APR_UWRITE | APR_UEXECUTE, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_stat(&finfo, "data/one", APR_FINFO_TYPE, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, APR_DIR, finfo.filetype);

    rv = fspr_stat(&finfo, "data/one/two", APR_FINFO_TYPE, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, APR_DIR, finfo.filetype);

    rv = fspr_stat(&finfo, "data/one/two/three", APR_FINFO_TYPE, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, APR_DIR, finfo.filetype);
}

static void test_remove(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_finfo_t finfo;

    rv = fspr_dir_remove("data/testdir", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_stat(&finfo, "data/testdir", APR_FINFO_TYPE, p);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_ENOENT(rv));
}

static void test_removeall_fail(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_dir_remove("data/one", p);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_ENOTEMPTY(rv));
}

static void test_removeall(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_dir_remove("data/one/two/three", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_dir_remove("data/one/two", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_dir_remove("data/one", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
}

static void test_remove_notthere(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_dir_remove("data/notthere", p);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_ENOENT(rv));
}

static void test_mkdir_twice(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_dir_make("data/testdir", APR_UREAD | APR_UWRITE | APR_UEXECUTE, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_dir_make("data/testdir", APR_UREAD | APR_UWRITE | APR_UEXECUTE, p);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_EEXIST(rv));

    rv = fspr_dir_remove("data/testdir", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
}

static void test_opendir(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_dir_t *dir;

    rv = fspr_dir_open(&dir, "data", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    fspr_dir_close(dir);
}

static void test_opendir_notthere(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_dir_t *dir;

    rv = fspr_dir_open(&dir, "notthere", p);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_ENOENT(rv));
}

static void test_closedir(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_dir_t *dir;

    rv = fspr_dir_open(&dir, "data", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_dir_close(dir);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
}

static void test_rewind(abts_case *tc, void *data)
{
    fspr_dir_t *dir;
    fspr_finfo_t first, second;

    APR_ASSERT_SUCCESS(tc, "fspr_dir_open failed", fspr_dir_open(&dir, "data", p));

    APR_ASSERT_SUCCESS(tc, "fspr_dir_read failed",
                       fspr_dir_read(&first, APR_FINFO_DIRENT, dir));

    APR_ASSERT_SUCCESS(tc, "fspr_dir_rewind failed", fspr_dir_rewind(dir));

    APR_ASSERT_SUCCESS(tc, "second fspr_dir_read failed",
                       fspr_dir_read(&second, APR_FINFO_DIRENT, dir));

    APR_ASSERT_SUCCESS(tc, "fspr_dir_close failed", fspr_dir_close(dir));

    ABTS_STR_EQUAL(tc, first.name, second.name);
}

/* Test for a (fixed) bug in fspr_dir_read().  This bug only happened
   in threadless cases. */
static void test_uncleared_errno(abts_case *tc, void *data)
{
    fspr_file_t *thefile = NULL;
    fspr_finfo_t finfo;
    fspr_int32_t finfo_flags = APR_FINFO_TYPE | APR_FINFO_NAME;
    fspr_dir_t *this_dir;
    fspr_status_t rv; 

    rv = fspr_dir_make("dir1", APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_dir_make("dir2", APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_file_open(&thefile, "dir1/file1",
                       APR_READ | APR_WRITE | APR_CREATE, APR_OS_DEFAULT, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_file_close(thefile);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    /* Try to remove dir1.  This should fail because it's not empty.
       However, on a platform with threads disabled (such as FreeBSD),
       `errno' will be set as a result. */
    rv = fspr_dir_remove("dir1", p);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_ENOTEMPTY(rv));
    
    /* Read `.' and `..' out of dir2. */
    rv = fspr_dir_open(&this_dir, "dir2", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_dir_read(&finfo, finfo_flags, this_dir);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_dir_read(&finfo, finfo_flags, this_dir);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    /* Now, when we attempt to do a third read of empty dir2, and the
       underlying system readdir() returns NULL, the old value of
       errno shouldn't cause a false alarm.  We should get an ENOENT
       back from fspr_dir_read, and *not* the old errno. */
    rv = fspr_dir_read(&finfo, finfo_flags, this_dir);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_ENOENT(rv));

    rv = fspr_dir_close(this_dir);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
		 
    /* Cleanup */
    rv = fspr_file_remove("dir1/file1", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_dir_remove("dir1", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = fspr_dir_remove("dir2", p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

}

static void test_rmkdir_nocwd(abts_case *tc, void *data)
{
    char *cwd, *path;
    fspr_status_t rv;

    APR_ASSERT_SUCCESS(tc, "make temp dir",
                       fspr_dir_make("dir3", APR_OS_DEFAULT, p));

    APR_ASSERT_SUCCESS(tc, "obtain cwd", fspr_filepath_get(&cwd, 0, p));

    APR_ASSERT_SUCCESS(tc, "determine path to temp dir",
                       fspr_filepath_merge(&path, cwd, "dir3", 0, p));

    APR_ASSERT_SUCCESS(tc, "change to temp dir", fspr_filepath_set(path, p));

    rv = fspr_dir_remove(path, p);
    /* Some platforms cannot remove a directory which is in use. */
    if (rv == APR_SUCCESS) {
        ABTS_ASSERT(tc, "fail to create dir",
                    fspr_dir_make_recursive("foobar", APR_OS_DEFAULT, 
                                           p) != APR_SUCCESS);
    }

    APR_ASSERT_SUCCESS(tc, "restore cwd", fspr_filepath_set(cwd, p));

    if (rv) {
        fspr_dir_remove(path, p);
        ABTS_NOT_IMPL(tc, "cannot remove in-use directory");
    }
}


abts_suite *testdir(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, test_mkdir, NULL);
    abts_run_test(suite, test_mkdir_recurs, NULL);
    abts_run_test(suite, test_remove, NULL);
    abts_run_test(suite, test_removeall_fail, NULL);
    abts_run_test(suite, test_removeall, NULL);
    abts_run_test(suite, test_remove_notthere, NULL);
    abts_run_test(suite, test_mkdir_twice, NULL);
    abts_run_test(suite, test_rmkdir_nocwd, NULL);

    abts_run_test(suite, test_rewind, NULL);

    abts_run_test(suite, test_opendir, NULL);
    abts_run_test(suite, test_opendir_notthere, NULL);
    abts_run_test(suite, test_closedir, NULL);
    abts_run_test(suite, test_uncleared_errno, NULL);

    return suite;
}

