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
#include "fspr_thread_proc.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"

#if APR_HAS_OTHER_CHILD

static char reasonstr[256];

static void ocmaint(int reason, void *data, int status)
{
    switch (reason) {
    case APR_OC_REASON_DEATH:
        fspr_cpystrn(reasonstr, "APR_OC_REASON_DEATH", 
                    strlen("APR_OC_REASON_DEATH") + 1);
        break;
    case APR_OC_REASON_LOST:
        fspr_cpystrn(reasonstr, "APR_OC_REASON_LOST", 
                    strlen("APR_OC_REASON_LOST") + 1);
        break;
    case APR_OC_REASON_UNWRITABLE:
        fspr_cpystrn(reasonstr, "APR_OC_REASON_UNWRITEABLE", 
                    strlen("APR_OC_REASON_UNWRITEABLE") + 1);
        break;
    case APR_OC_REASON_RESTART:
        fspr_cpystrn(reasonstr, "APR_OC_REASON_RESTART", 
                    strlen("APR_OC_REASON_RESTART") + 1);
        break;
    }
}

#ifndef SIGKILL
#define SIGKILL 1
#endif

/* It would be great if we could stress this stuff more, and make the test
 * more granular.
 */
static void test_child_kill(abts_case *tc, void *data)
{
    fspr_file_t *std = NULL;
    fspr_proc_t newproc;
    fspr_procattr_t *procattr = NULL;
    const char *args[3];
    fspr_status_t rv;

    args[0] = fspr_pstrdup(p, "occhild" EXTENSION);
    args[1] = fspr_pstrdup(p, "-X");
    args[2] = NULL;

    rv = fspr_procattr_create(&procattr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_procattr_io_set(procattr, APR_FULL_BLOCK, APR_NO_PIPE, 
                             APR_NO_PIPE);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_proc_create(&newproc, "./occhild" EXTENSION, args, NULL, procattr, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, newproc.in);
    ABTS_PTR_EQUAL(tc, NULL, newproc.out);
    ABTS_PTR_EQUAL(tc, NULL, newproc.err);

    std = newproc.in;

    fspr_proc_other_child_register(&newproc, ocmaint, NULL, std, p);

    fspr_sleep(fspr_time_from_sec(1));
    rv = fspr_proc_kill(&newproc, SIGKILL);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    
    /* allow time for things to settle... */
    fspr_sleep(fspr_time_from_sec(3));
    
    fspr_proc_other_child_refresh_all(APR_OC_REASON_RUNNING);
    ABTS_STR_EQUAL(tc, "APR_OC_REASON_DEATH", reasonstr);
}    
#else

static void oc_not_impl(abts_case *tc, void *data)
{
    ABTS_NOT_IMPL(tc, "Other child logic not implemented on this platform");
}
#endif

abts_suite *testoc(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

#if !APR_HAS_OTHER_CHILD
    abts_run_test(suite, oc_not_impl, NULL);
#else

    abts_run_test(suite, test_child_kill, NULL); 

#endif
    return suite;
}

