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
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_user.h"

#if APR_HAS_USER
static void uid_current(abts_case *tc, void *data)
{
    fspr_uid_t uid;
    fspr_gid_t gid;

    APR_ASSERT_SUCCESS(tc, "fspr_uid_current failed",
                       fspr_uid_current(&uid, &gid, p));
}

static void username(abts_case *tc, void *data)
{
    fspr_uid_t uid;
    fspr_gid_t gid;
    fspr_uid_t retreived_uid;
    fspr_gid_t retreived_gid;
    char *uname = NULL;

    APR_ASSERT_SUCCESS(tc, "fspr_uid_current failed",
                       fspr_uid_current(&uid, &gid, p));
   
    APR_ASSERT_SUCCESS(tc, "fspr_uid_name_get failed",
                       fspr_uid_name_get(&uname, uid, p));
    ABTS_PTR_NOTNULL(tc, uname);

    APR_ASSERT_SUCCESS(tc, "fspr_uid_get failed",
                       fspr_uid_get(&retreived_uid, &retreived_gid, uname, p));

    APR_ASSERT_SUCCESS(tc, "fspr_uid_compare failed",
                       fspr_uid_compare(uid, retreived_uid));
#ifdef WIN32
    /* ### this fudge was added for Win32 but makes the test return NotImpl
     * on Unix if run as root, when !gid is also true. */
    if (!gid || !retreived_gid) {
        /* The function had no way to recover the gid (this would have been
         * an ENOTIMPL if fspr_uid_ functions didn't try to double-up and
         * also return fspr_gid_t values, which was bogus.
         */
        if (!gid) {
            ABTS_NOT_IMPL(tc, "Groups from fspr_uid_current");
        }
        else {
            ABTS_NOT_IMPL(tc, "Groups from fspr_uid_get");
        }        
    }
    else {
#endif
        APR_ASSERT_SUCCESS(tc, "fspr_gid_compare failed",
                           fspr_gid_compare(gid, retreived_gid));
#ifdef WIN32
    }
#endif
}

static void groupname(abts_case *tc, void *data)
{
    fspr_uid_t uid;
    fspr_gid_t gid;
    fspr_gid_t retreived_gid;
    char *gname = NULL;

    APR_ASSERT_SUCCESS(tc, "fspr_uid_current failed",
                       fspr_uid_current(&uid, &gid, p));

    APR_ASSERT_SUCCESS(tc, "fspr_gid_name_get failed",
                       fspr_gid_name_get(&gname, gid, p));
    ABTS_PTR_NOTNULL(tc, gname);

    APR_ASSERT_SUCCESS(tc, "fspr_gid_get failed",
                       fspr_gid_get(&retreived_gid, gname, p));

    APR_ASSERT_SUCCESS(tc, "fspr_gid_compare failed",
                       fspr_gid_compare(gid, retreived_gid));
}

#ifndef WIN32

static void fail_userinfo(abts_case *tc, void *data)
{
    fspr_uid_t uid;
    fspr_gid_t gid;
    fspr_status_t rv;
    char *tmp;

    errno = 0;
    gid = uid = 9999999;
    tmp = NULL;
    rv = fspr_uid_name_get(&tmp, uid, p);
    ABTS_ASSERT(tc, "fspr_uid_name_get should fail or "
                "return a user name",
                rv != APR_SUCCESS || tmp != NULL);

    errno = 0;
    tmp = NULL;
    rv = fspr_gid_name_get(&tmp, gid, p);
    ABTS_ASSERT(tc, "fspr_gid_name_get should fail or "
                "return a group name",
                rv != APR_SUCCESS || tmp != NULL);

    gid = 424242;
    errno = 0;
    rv = fspr_gid_get(&gid, "I_AM_NOT_A_GROUP", p);
    ABTS_ASSERT(tc, "fspr_gid_get should fail or "
                "set a group number",
                rv != APR_SUCCESS || gid == 424242);

    gid = uid = 424242;
    errno = 0;
    rv = fspr_uid_get(&uid, &gid, "I_AM_NOT_A_USER", p);
    ABTS_ASSERT(tc, "fspr_gid_get should fail or "
                "set a user and group number",
                rv != APR_SUCCESS || uid == 424242 || gid == 4242442);

    errno = 0;
    tmp = NULL;
    rv = fspr_uid_homepath_get(&tmp, "I_AM_NOT_A_USER", p);
    ABTS_ASSERT(tc, "fspr_uid_homepath_get should fail or "
                "set a path name",
                rv != APR_SUCCESS || tmp != NULL);
}

#else
static void fail_userinfo(abts_case *tc, void *data)
{
    ABTS_NOT_IMPL(tc, "Users are not opaque integers on this platform");
}
#endif

#else
static void users_not_impl(abts_case *tc, void *data)
{
    ABTS_NOT_IMPL(tc, "Users not implemented on this platform");
}
#endif

abts_suite *testuser(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

#if !APR_HAS_USER
    abts_run_test(suite, users_not_impl, NULL);
#else
    abts_run_test(suite, uid_current, NULL);
    abts_run_test(suite, username, NULL);
    abts_run_test(suite, groupname, NULL);
    abts_run_test(suite, fail_userinfo, NULL);
#endif

    return suite;
}
