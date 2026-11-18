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
#include "fspr_mmap.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_file_io.h"
#include "fspr_strings.h"

/* hmmm, what is a truly portable define for the max path
 * length on a platform?
 */
#define PATH_LEN 255
#define TEST_STRING "This is the MMAP data file."APR_EOL_STR

#if !APR_HAS_MMAP
static void not_implemented(abts_case *tc, void *data)
{
    ABTS_NOT_IMPL(tc, "User functions");
}

#else

static fspr_mmap_t *themmap = NULL;
static fspr_file_t *thefile = NULL;
static char *file1;
static fspr_finfo_t finfo;
static int fsize;

static void create_filename(abts_case *tc, void *data)
{
    char *oldfileptr;

    fspr_filepath_get(&file1, 0, p);
#ifndef NETWARE
#ifdef WIN32
    ABTS_TRUE(tc, file1[1] == ':');
#else
    ABTS_TRUE(tc, file1[0] == '/');
#endif
#endif
    ABTS_TRUE(tc, file1[strlen(file1) - 1] != '/');

    oldfileptr = file1;
    file1 = fspr_pstrcat(p, file1,"/data/mmap_datafile.txt" ,NULL);
    ABTS_TRUE(tc, oldfileptr != file1);
}

static void test_file_close(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_file_close(thefile);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
}
   
static void test_file_open(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_file_open(&thefile, file1, APR_READ, APR_UREAD | APR_GREAD, p);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
    ABTS_PTR_NOTNULL(tc, thefile);
}
   
static void test_get_filesize(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_file_info_get(&finfo, APR_FINFO_NORM, thefile);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
    ABTS_ASSERT(tc, "File size mismatch", fsize == finfo.size);
}

static void test_mmap_create(abts_case *tc, void *data)
{
    fspr_status_t rv;

    rv = fspr_mmap_create(&themmap, thefile, 0, (fspr_size_t) finfo.size, 
                                APR_MMAP_READ, p);
    ABTS_PTR_NOTNULL(tc, themmap);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
}

static void test_mmap_contents(abts_case *tc, void *data)
{
    
    ABTS_PTR_NOTNULL(tc, themmap);
    ABTS_PTR_NOTNULL(tc, themmap->mm);
    ABTS_INT_EQUAL(tc, fsize, themmap->size);

    /* Must use nEquals since the string is not guaranteed to be NULL terminated */
    ABTS_STR_NEQUAL(tc, themmap->mm, TEST_STRING, fsize);
}

static void test_mmap_delete(abts_case *tc, void *data)
{
    fspr_status_t rv;

    ABTS_PTR_NOTNULL(tc, themmap);
    rv = fspr_mmap_delete(themmap);
    ABTS_INT_EQUAL(tc, rv, APR_SUCCESS);
}

static void test_mmap_offset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    void *addr;

    ABTS_PTR_NOTNULL(tc, themmap);
    rv = fspr_mmap_offset(&addr, themmap, 5);

    /* Must use nEquals since the string is not guaranteed to be NULL terminated */
    ABTS_STR_NEQUAL(tc, addr, TEST_STRING + 5, fsize-5);
}
#endif

abts_suite *testmmap(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

#if APR_HAS_MMAP    
    fsize = strlen(TEST_STRING);

    abts_run_test(suite, create_filename, NULL);
    abts_run_test(suite, test_file_open, NULL);
    abts_run_test(suite, test_get_filesize, NULL);
    abts_run_test(suite, test_mmap_create, NULL);
    abts_run_test(suite, test_mmap_contents, NULL);
    abts_run_test(suite, test_mmap_offset, NULL);
    abts_run_test(suite, test_mmap_delete, NULL);
    abts_run_test(suite, test_file_close, NULL);
#else
    abts_run_test(suite, not_implemented, NULL);
#endif

    return suite;
}

