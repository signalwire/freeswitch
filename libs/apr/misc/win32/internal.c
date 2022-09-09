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

#include "fspr_private.h"

#include "fspr_arch_misc.h"
#include "fspr_arch_file_io.h"
#include <crtdbg.h>
#include <assert.h>

/* This module is the source of -static- helper functions that are
 * entirely internal to apr.  If the fn is exported - it does not
 * belong here.
 *
 * Namespace decoration is still required to protect us from symbol
 * clashes in static linkages.
 */


/* Shared by fspr_app.c and start.c 
 *
 * An internal apr function to convert an array of strings (either
 * a counted or NULL terminated list, such as an argv[argc] or env[]
 * list respectively) from wide Unicode strings to narrow utf-8 strings.
 * These are allocated from the MSVCRT's _CRT_BLOCK to trick the system
 * into trusting our store.
 */
int fspr_wastrtoastr(char const * const * *retarr, 
                    wchar_t const * const *arr, int args)
{
    fspr_size_t elesize = 0;
    char **newarr;
    char *elements;
    char *ele;
    int arg;

    if (args < 0) {
        for (args = 0; arr[args]; ++args)
            ;
    }

    newarr = _malloc_dbg((args + 1) * sizeof(char *),
                         _CRT_BLOCK, __FILE__, __LINE__);

    for (arg = 0; arg < args; ++arg) {
        newarr[arg] = (void*)(wcslen(arr[arg]) + 1);
        elesize += (fspr_size_t)newarr[arg];
    }

    /* This is a safe max allocation, we will realloc after
     * processing and return the excess to the free store.
     * 3 ucs bytes hold any single wchar_t value (16 bits)
     * 4 ucs bytes will hold a wchar_t pair value (20 bits)
     */
    elesize = elesize * 3 + 1;
    ele = elements = _malloc_dbg(elesize * sizeof(char), 
                                 _CRT_BLOCK, __FILE__, __LINE__);

    for (arg = 0; arg < args; ++arg) {
        fspr_size_t len = (fspr_size_t)newarr[arg];
        fspr_size_t newlen = elesize;

        newarr[arg] = ele;
        (void)fspr_conv_ucs2_to_utf8(arr[arg], &len,
                                    newarr[arg], &elesize);

        newlen -= elesize;
        ele += newlen;
        assert(elesize && (len == 0));
    }

    newarr[arg] = NULL;
    *(ele++) = '\0';

    /* Return to the free store if the heap realloc is the least bit optimized
     */
    ele = _realloc_dbg(elements, ele - elements, 
                       _CRT_BLOCK, __FILE__, __LINE__);

    if (ele != elements) {
        fspr_size_t diff = ele - elements;
        for (arg = 0; arg < args; ++arg) {
            newarr[arg] += diff;
        }
    }

    *retarr = newarr;
    return args;
}
