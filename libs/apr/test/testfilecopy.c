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
#include "fspr_file_io.h"
#include "fspr_file_info.h"
#include "fspr_errno.h"
#include "fspr_pools.h"

static void copy_helper(abts_case *tc, const char *from, const char * to,
                        fspr_fileperms_t perms, int append, fspr_pool_t *p)
{
    fspr_status_t rv;
    fspr_status_t dest_rv;
    fspr_finfo_t copy;
    fspr_finfo_t orig;
    fspr_finfo_t dest;
    
    dest_rv = fspr_stat(&dest, to, APR_FINFO_SIZE, p);
    
    if (!append) {
        rv = fspr_file_copy(from, to, perms, p);
    }
    else {
        rv = fspr_file_append(from, to, perms, p);
    }
    APR_ASSERT_SUCCESS(tc, "Error copying file", rv);

    rv = fspr_stat(&orig, from, APR_FINFO_SIZE, p);
    APR_ASSERT_SUCCESS(tc, "Couldn't stat original file", rv);

    rv = fspr_stat(&copy, to, APR_FINFO_SIZE, p);
    APR_ASSERT_SUCCESS(tc, "Couldn't stat copy file", rv);

    if (!append) {
        ABTS_ASSERT(tc, "File size differs", orig.size == copy.size);
    }
    else {
        ABTS_ASSERT(tc, "File size differs", 
                                   ((dest_rv == APR_SUCCESS) 
                                     ? dest.size : 0) + orig.size == copy.size);
    }
}

static void copy_short_file(abts_case *tc, void *data)
{
    fspr_status_t rv;

    /* make absolutely sure that the dest file doesn't exist. */
    fspr_file_remove("data/file_copy.txt", p);
    
    copy_helper(tc, "data/file_datafile.txt", "data/file_copy.txt", 
                APR_FILE_SOURCE_PERMS, 0, p);
    rv = fspr_file_remove("data/file_copy.txt", p);
    APR_ASSERT_SUCCESS(tc, "Couldn't remove copy file", rv);
}

static void copy_over_existing(abts_case *tc, void *data)
{
    fspr_status_t rv;
    
    /* make absolutely sure that the dest file doesn't exist. */
    fspr_file_remove("data/file_copy.txt", p);
    
    /* This is a cheat.  I don't want to create a new file, so I just copy
     * one file, then I copy another.  If the second copy succeeds, then
     * this works.
     */
    copy_helper(tc, "data/file_datafile.txt", "data/file_copy.txt", 
                APR_FILE_SOURCE_PERMS, 0, p);
    
    copy_helper(tc, "data/mmap_datafile.txt", "data/file_copy.txt", 
                APR_FILE_SOURCE_PERMS, 0, p);
  
    rv = fspr_file_remove("data/file_copy.txt", p);
    APR_ASSERT_SUCCESS(tc, "Couldn't remove copy file", rv);
}

static void append_nonexist(abts_case *tc, void *data)
{
    fspr_status_t rv;

    /* make absolutely sure that the dest file doesn't exist. */
    fspr_file_remove("data/file_copy.txt", p);

    copy_helper(tc, "data/file_datafile.txt", "data/file_copy.txt", 
                APR_FILE_SOURCE_PERMS, 0, p);
    rv = fspr_file_remove("data/file_copy.txt", p);
    APR_ASSERT_SUCCESS(tc, "Couldn't remove copy file", rv);
}

static void append_exist(abts_case *tc, void *data)
{
    fspr_status_t rv;
    
    /* make absolutely sure that the dest file doesn't exist. */
    fspr_file_remove("data/file_copy.txt", p);
    
    /* This is a cheat.  I don't want to create a new file, so I just copy
     * one file, then I copy another.  If the second copy succeeds, then
     * this works.
     */
    copy_helper(tc, "data/file_datafile.txt", "data/file_copy.txt", 
                APR_FILE_SOURCE_PERMS, 0, p);
    
    copy_helper(tc, "data/mmap_datafile.txt", "data/file_copy.txt", 
                APR_FILE_SOURCE_PERMS, 1, p);
  
    rv = fspr_file_remove("data/file_copy.txt", p);
    APR_ASSERT_SUCCESS(tc, "Couldn't remove copy file", rv);
}

abts_suite *testfilecopy(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, copy_short_file, NULL);
    abts_run_test(suite, copy_over_existing, NULL);

    abts_run_test(suite, append_nonexist, NULL);
    abts_run_test(suite, append_exist, NULL);

    return suite;
}

