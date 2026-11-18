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

#include "fspr_arch_dso.h"
#include "fspr_strings.h"
#include "fspr_private.h"
#include "fspr_arch_file_io.h"
#include "fspr_arch_utf8.h"

#if APR_HAS_DSO

APR_DECLARE(fspr_status_t) fspr_os_dso_handle_put(fspr_dso_handle_t **aprdso,
                                                fspr_os_dso_handle_t osdso,
                                                fspr_pool_t *pool)
{
    *aprdso = fspr_pcalloc(pool, sizeof **aprdso);
    (*aprdso)->handle = osdso;
    (*aprdso)->cont = pool;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_dso_handle_get(fspr_os_dso_handle_t *osdso,
                                                fspr_dso_handle_t *aprdso)
{
    *osdso = aprdso->handle;
    return APR_SUCCESS;
}

static fspr_status_t dso_cleanup(void *thedso)
{
    fspr_dso_handle_t *dso = thedso;

    if (dso->handle != NULL && !FreeLibrary(dso->handle)) {
        return fspr_get_os_error();
    }
    dso->handle = NULL;

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_dso_load(struct fspr_dso_handle_t **res_handle, 
                                       const char *path, fspr_pool_t *ctx)
{
    HINSTANCE os_handle;
    fspr_status_t rv;
#ifndef _WIN32_WCE
    UINT em;
#endif

#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE 
    {
        fspr_wchar_t wpath[APR_PATH_MAX];
        if ((rv = utf8_to_unicode_path(wpath, sizeof(wpath) 
                                            / sizeof(fspr_wchar_t), path))
                != APR_SUCCESS) {
            *res_handle = fspr_pcalloc(ctx, sizeof(**res_handle));
            return ((*res_handle)->load_error = rv);
        }
        /* Prevent ugly popups from killing our app */
#ifndef _WIN32_WCE
        em = SetErrorMode(SEM_FAILCRITICALERRORS);
#endif
        os_handle = LoadLibraryExW(wpath, NULL, 0);
        if (!os_handle)
            os_handle = LoadLibraryExW(wpath, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!os_handle)
            rv = fspr_get_os_error();
#ifndef _WIN32_WCE
        SetErrorMode(em);
#endif
    }
#endif /* APR_HAS_UNICODE_FS */
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        char fspec[APR_PATH_MAX], *p = fspec;
        /* Must convert path from / to \ notation.
         * Per PR2555, the LoadLibraryEx function is very picky about slashes.
         * Debugging on NT 4 SP 6a reveals First Chance Exception within NTDLL.
         * LoadLibrary in the MS PSDK also reveals that it -explicitly- states
         * that backslashes must be used for the LoadLibrary family of calls.
         */
        fspr_cpystrn(fspec, path, sizeof(fspec));
        while ((p = strchr(p, '/')) != NULL)
            *p = '\\';
        
        /* Prevent ugly popups from killing our app */
        em = SetErrorMode(SEM_FAILCRITICALERRORS);
        os_handle = LoadLibraryEx(path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
        if (!os_handle)
            os_handle = LoadLibraryEx(path, NULL, 0);
        if (!os_handle)
            rv = fspr_get_os_error();
        else
            rv = APR_SUCCESS;
        SetErrorMode(em);
    }
#endif

    *res_handle = fspr_pcalloc(ctx, sizeof(**res_handle));
    (*res_handle)->cont = ctx;

    if (rv) {
        return ((*res_handle)->load_error = rv);
    }

    (*res_handle)->handle = (void*)os_handle;
    (*res_handle)->load_error = APR_SUCCESS;

    fspr_pool_cleanup_register(ctx, *res_handle, dso_cleanup, fspr_pool_cleanup_null);

    return APR_SUCCESS;
}
    
APR_DECLARE(fspr_status_t) fspr_dso_unload(struct fspr_dso_handle_t *handle)
{
    return fspr_pool_cleanup_run(handle->cont, handle, dso_cleanup);
}

APR_DECLARE(fspr_status_t) fspr_dso_sym(fspr_dso_handle_sym_t *ressym, 
                         struct fspr_dso_handle_t *handle, 
                         const char *symname)
{
#ifdef _WIN32_WCE
    fspr_size_t symlen = strlen(symname) + 1;
    fspr_size_t wsymlen = 256;
    fspr_wchar_t wsymname[256];
    fspr_status_t rv;

    rv = fspr_conv_utf8_to_ucs2(wsymname, &wsymlen, symname, &symlen);
    if (rv != APR_SUCCESS) {
        return rv;
    }
    else if (symlen) {
        return APR_ENAMETOOLONG;
    }

    *ressym = (fspr_dso_handle_sym_t)GetProcAddressW(handle->handle, wsymname);
#else
    *ressym = (fspr_dso_handle_sym_t)GetProcAddress(handle->handle, symname);
#endif
    if (!*ressym) {
        return fspr_get_os_error();
    }
    return APR_SUCCESS;
}

APR_DECLARE(const char *) fspr_dso_error(fspr_dso_handle_t *dso, char *buf, fspr_size_t bufsize)
{
    return fspr_strerror(dso->load_error, buf, bufsize);
}

#endif
