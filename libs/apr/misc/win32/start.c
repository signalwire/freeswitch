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
#include "fspr_general.h"
#include "fspr_pools.h"
#include "fspr_signal.h"
#include "ShellAPI.h"

#include "fspr_arch_misc.h"       /* for WSAHighByte / WSALowByte */
#include "wchar.h"
#include "fspr_arch_file_io.h"
#include "crtdbg.h"
#include "assert.h"

/* This symbol is _private_, although it must be exported.
 */
int APR_DECLARE_DATA fspr_app_init_complete = 0;

/* Used by fspr_app_initialize to reprocess the environment
 *
 * An internal apr function to convert a double-null terminated set
 * of single-null terminated strings from wide Unicode to narrow utf-8
 * as a list of strings.  These are allocated from the MSVCRT's
 * _CRT_BLOCK to trick the system into trusting our store.
 */
static int warrsztoastr(const char * const * *retarr,
                        const wchar_t * arrsz, int args)
{
    const fspr_wchar_t *wch;
    fspr_size_t totlen;
    fspr_size_t newlen;
    fspr_size_t wsize;
    char **newarr;
    int arg;

    if (args < 0) {
        for (args = 1, wch = arrsz; wch[0] || wch[1]; ++wch)
            if (!*wch)
                ++args;
    }
    wsize = 1 + wch - arrsz;

    newarr = _malloc_dbg((args + 1) * sizeof(char *),
                         _CRT_BLOCK, __FILE__, __LINE__);

    /* This is a safe max allocation, we will realloc after
     * processing and return the excess to the free store.
     * 3 ucs bytes hold any single wchar_t value (16 bits)
     * 4 ucs bytes will hold a wchar_t pair value (20 bits)
     */
    newlen = totlen = wsize * 3 + 1;
    newarr[0] = _malloc_dbg(newlen * sizeof(char),
                            _CRT_BLOCK, __FILE__, __LINE__);

    (void)fspr_conv_ucs2_to_utf8(arrsz, &wsize,
                                newarr[0], &newlen);

    assert(newlen && !wsize);
    /* Return to the free store if the heap realloc is the least bit optimized
     */
    newarr[0] = _realloc_dbg(newarr[0], totlen - newlen,
                             _CRT_BLOCK, __FILE__, __LINE__);

    for (arg = 1; arg < args; ++arg) {
        newarr[arg] = newarr[arg - 1] + 2;
        while (*(newarr[arg]++)) {
            /* continue */;
        }
    }

    newarr[arg] = NULL;

    *retarr = newarr;
    return args;
}

/* Reprocess the arguments to main() for a completely apr-ized application
 */

APR_DECLARE(fspr_status_t) fspr_app_initialize(int *argc,
                                             const char * const * *argv,
                                             const char * const * *env)
{
    fspr_status_t rv = fspr_initialize();

    if (rv != APR_SUCCESS) {
        return rv;
    }

#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        fspr_wchar_t **wstrs;
        fspr_wchar_t *sysstr;
        int wstrc;
        int dupenv;

        if (fspr_app_init_complete) {
            return rv;
        }

        fspr_app_init_complete = 1;

        sysstr = GetCommandLineW();
        if (sysstr) {
            wstrs = CommandLineToArgvW(sysstr, &wstrc);
            if (wstrs) {
                *argc = fspr_wastrtoastr(argv, wstrs, wstrc);
                GlobalFree(wstrs);
            }
        }

        sysstr = GetEnvironmentStringsW();
        dupenv = warrsztoastr(&_environ, sysstr, -1);

        if (env) {
            *env = _malloc_dbg((dupenv + 1) * sizeof (char *),
                               _CRT_BLOCK, __FILE__, __LINE__ );
            memcpy((void*)*env, _environ, (dupenv + 1) * sizeof (char *));
        }
        else {
        }

        FreeEnvironmentStringsW(sysstr);

        /* MSVCRT will attempt to maintain the wide environment calls
         * on _putenv(), which is bogus if we've passed a non-ascii
         * string to _putenv(), since they use MultiByteToWideChar
         * and breaking the implicit utf-8 assumption we've built.
         *
         * Reset _wenviron for good measure.
         */
        if (_wenviron) {
            fspr_wchar_t **wenv = _wenviron;
            _wenviron = NULL;
            free(wenv);
        }

    }
#endif
    return rv;
}

static int initialized = 0;

/* Provide to win32/thread.c */
extern DWORD tls_fspr_thread;

APR_DECLARE(fspr_status_t) fspr_initialize(void)
{
    fspr_pool_t *pool;
    fspr_status_t status;
    int iVersionRequested;
    WSADATA wsaData;
    int err;
    fspr_oslevel_e osver;

    if (initialized++) {
        return APR_SUCCESS;
    }

    /* Initialize fspr_os_level global */
    if (fspr_get_oslevel(&osver) != APR_SUCCESS) {
        return APR_EEXIST;
    }

    tls_fspr_thread = TlsAlloc();
    if ((status = fspr_pool_initialize()) != APR_SUCCESS)
        return status;

    if (fspr_pool_create(&pool, NULL) != APR_SUCCESS) {
        return APR_ENOPOOL;
    }

    fspr_pool_tag(pool, "fspr_initialize");

    iVersionRequested = MAKEWORD(WSAHighByte, WSALowByte);
    err = WSAStartup((WORD) iVersionRequested, &wsaData);
    if (err) {
        return err;
    }
    if (LOBYTE(wsaData.wVersion) != WSAHighByte ||
        HIBYTE(wsaData.wVersion) != WSALowByte) {
        WSACleanup();
        return APR_EEXIST;
    }

    fspr_signal_init(pool);

    return APR_SUCCESS;
}

APR_DECLARE_NONSTD(void) fspr_terminate(void)
{
    initialized--;
    if (initialized) {
        return;
    }
    fspr_pool_terminate();

    WSACleanup();

    TlsFree(tls_fspr_thread);
}

APR_DECLARE(void) fspr_terminate2(void)
{
    fspr_terminate();
}
