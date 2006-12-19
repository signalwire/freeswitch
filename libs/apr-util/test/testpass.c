/* Copyright 2000-2005 The Apache Software Foundation or its licensors, as
 * applicable.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "apr_errno.h"
#include "apr_strings.h"
#include "apr_file_io.h"
#include "apr_thread_proc.h"
#include "apr_md5.h"
#include "apr_sha1.h"

#include "abts.h"
#include "testutil.h"

static struct {
    const char *password;
    const char *hash;
} passwords[] =
{
/*
  passwords and hashes created with Apache's htpasswd utility like this:
  
  htpasswd -c -b passwords pass1 pass1
  htpasswd -b passwords pass2 pass2
  htpasswd -b passwords pass3 pass3
  htpasswd -b passwords pass4 pass4
  htpasswd -b passwords pass5 pass5
  htpasswd -b passwords pass6 pass6
  htpasswd -b passwords pass7 pass7
  htpasswd -b passwords pass8 pass8
  (insert Perl one-liner to convert to initializer :) )
 */
    {"pass1", "1fWDc9QWYCWrQ"},
    {"pass2", "1fiGx3u7QoXaM"},
    {"pass3", "1fzijMylTiwCs"},
    {"pass4", "nHUYc8U2UOP7s"},
    {"pass5", "nHpETGLGPwAmA"},
    {"pass6", "nHbsbWmJ3uyhc"},
    {"pass7", "nHQ3BbF0Y9vpI"},
    {"pass8", "nHZA1rViSldQk"}
};
static int num_passwords = sizeof(passwords) / sizeof(passwords[0]);

static void test_crypt(abts_case *tc, void *data)
{
    int i;

    for (i = 0; i < num_passwords; i++) {
        apr_assert_success(tc, "check for valid password",
                           apr_password_validate(passwords[i].password,
                                                 passwords[i].hash));
    }
}

#if APR_HAS_THREADS

static void * APR_THREAD_FUNC testing_thread(apr_thread_t *thd,
                                             void *data)
{
    abts_case *tc = data;
    int i;

    for (i = 0; i < 100; i++) {
        test_crypt(tc, NULL);
    }

    return APR_SUCCESS;
}

/* test for threadsafe crypt() */
static void test_threadsafe(abts_case *tc, void *data)
{
#define NUM_THR 20
    apr_thread_t *my_threads[NUM_THR];
    int i;
    apr_status_t rv;
    
    for (i = 0; i < NUM_THR; i++) {
        apr_assert_success(tc, "create test thread",
                           apr_thread_create(&my_threads[i], NULL, 
                                             testing_thread, tc, p));
    }

    for (i = 0; i < NUM_THR; i++) {
        apr_thread_join(&rv, my_threads[i]);
    }
}
#endif

static void test_shapass(abts_case *tc, void *data)
{
    const char *pass = "hellojed";
    char hash[100];

    apr_sha1_base64(pass, strlen(pass), hash);

    apr_assert_success(tc, "SHA1 password validated",
                       apr_password_validate(pass, hash));
}

static void test_md5pass(abts_case *tc, void *data)
{
    const char *pass = "hellojed", *salt = "sardine";
    char hash[100];

    apr_md5_encode(pass, salt, hash, sizeof hash);

    apr_assert_success(tc, "MD5 password validated",
                       apr_password_validate(pass, hash));
}

abts_suite *testpass(abts_suite *suite)
{
    suite = ADD_SUITE(suite);

    abts_run_test(suite, test_crypt, NULL);
#if APR_HAS_THREADS
    abts_run_test(suite, test_threadsafe, NULL);
#endif
    abts_run_test(suite, test_shapass, NULL);
    abts_run_test(suite, test_md5pass, NULL);
    
    return suite;
}
