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

#include <stdio.h>
#include <stdlib.h>

#include "apr.h"
#include "apr_errno.h"
#include "apr_general.h"
#include "apr_strings.h"
#include "apr_xlate.h"

static const char test_utf8[] = "Edelwei\xc3\x9f";
static const char test_utf7[] = "Edelwei+AN8-";
static const char test_latin1[] = "Edelwei\xdf";
static const char test_latin2[] = "Edelwei\xdf";


static int check_status (apr_status_t status, const char *msg)
{
    if (status)
    {
        static char buf[1024];
        printf("ERROR: %s\n      %s\n", msg,
               apr_strerror(status, buf, sizeof(buf)));
        return 1;
    }
    return 0;
}

static int test_conversion (apr_xlate_t *convset,
                            const char *inbuf,
                            const char *expected)
{
    static char buf[1024];
    int retcode = 0;
    apr_size_t inbytes_left = strlen(inbuf);
    apr_size_t outbytes_left = sizeof(buf) - 1;
    apr_status_t status = apr_xlate_conv_buffer(convset,
                                                inbuf,
                                                &inbytes_left,
                                                buf,
                                                &outbytes_left);
    if (status == APR_SUCCESS) {
        status = apr_xlate_conv_buffer(convset, NULL, NULL,
                                       buf + sizeof(buf) - outbytes_left - 1,
                                       &outbytes_left);
    }
    buf[sizeof(buf) - outbytes_left - 1] = '\0';
    retcode |= check_status(status, "apr_xlate_conv_buffer");
    if ((!status || APR_STATUS_IS_INCOMPLETE(status))
        && strcmp(buf, expected))
    {
        printf("ERROR: expected: '%s'\n       actual:   '%s'"
               "\n       inbytes_left: %"APR_SIZE_T_FMT"\n",
               expected, buf, inbytes_left);
        retcode |= 1;
    }
    return retcode;
}

static int one_test (const char *cs1, const char *cs2,
                     const char *str1, const char *str2,
                     apr_pool_t *pool)
{
    apr_xlate_t *convset;
    const char *msg = apr_psprintf(pool, "apr_xlate_open(%s, %s)", cs2, cs1);
    int retcode = check_status(apr_xlate_open(&convset, cs2, cs1, pool), msg);
    if (!retcode)
    {
        retcode |= test_conversion(convset, str1, str2);
        retcode |= check_status(apr_xlate_close(convset), "apr_xlate_close");
    }
    printf("%s:  %s -> %s\n", (retcode ? "FAIL" : "PASS"), cs1, cs2);
    return retcode;
}


int main (int argc, char **argv)
{
    apr_pool_t *pool;
    int retcode = 0;

#ifndef APR_HAS_XLATE
    puts("SKIP: apr_xlate not implemented");
    return 0;
#endif

    apr_initialize();
    atexit(apr_terminate);
    apr_pool_create(&pool, NULL);

    /* 1. Identity transformation: UTF-8 -> UTF-8 */
    retcode |= one_test("UTF-8", "UTF-8", test_utf8, test_utf8, pool);

    /* 2. UTF-8 <-> ISO-8859-1 */
    retcode |= one_test("UTF-8", "ISO-8859-1", test_utf8, test_latin1, pool);
    retcode |= one_test("ISO-8859-1", "UTF-8", test_latin1, test_utf8, pool);

    /* 3. ISO-8859-1 <-> ISO-8859-2, identity */
    retcode |= one_test("ISO-8859-1", "ISO-8859-2",
                        test_latin1, test_latin2, pool);
    retcode |= one_test("ISO-8859-2", "ISO-8859-1",
                        test_latin2, test_latin1, pool);

    /* 4. Transformation using charset aliases */
    retcode |= one_test("UTF-8", "UTF-7", test_utf8, test_utf7, pool);
    retcode |= one_test("UTF-7", "UTF-8", test_utf7, test_utf8, pool);

    return retcode;
}
