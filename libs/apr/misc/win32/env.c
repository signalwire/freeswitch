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

#define APR_WANT_STRFUNC
#include "fspr_want.h"
#include "fspr.h"
#include "fspr_arch_misc.h"
#include "fspr_arch_utf8.h"
#include "fspr_env.h"
#include "fspr_errno.h"
#include "fspr_pools.h"
#include "fspr_strings.h"


#if APR_HAS_UNICODE_FS
static fspr_status_t widen_envvar_name (fspr_wchar_t *buffer,
                                       fspr_size_t bufflen,
                                       const char *envvar)
{
    fspr_size_t inchars;
    fspr_status_t status;

    inchars = strlen(envvar) + 1;
    status = fspr_conv_utf8_to_ucs2(envvar, &inchars, buffer, &bufflen);
    if (status == APR_INCOMPLETE)
        status = APR_ENAMETOOLONG;

    return status;
}
#endif


APR_DECLARE(fspr_status_t) fspr_env_get(char **value,
                                      const char *envvar,
                                      fspr_pool_t *pool)
{
    char *val = NULL;
    DWORD size;

#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        fspr_wchar_t wenvvar[APR_PATH_MAX];
        fspr_size_t inchars, outchars;
        fspr_wchar_t *wvalue, dummy;
        fspr_status_t status;

        status = widen_envvar_name(wenvvar, APR_PATH_MAX, envvar);
        if (status)
            return status;

        SetLastError(0);
        size = GetEnvironmentVariableW(wenvvar, &dummy, 0);
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
            /* The environment variable doesn't exist. */
            return APR_ENOENT;

        if (size == 0) {
            /* The environment value exists, but is zero-length. */
            *value = fspr_pstrdup(pool, "");
            return APR_SUCCESS;
        }

        wvalue = fspr_palloc(pool, size * sizeof(*wvalue));
        size = GetEnvironmentVariableW(wenvvar, wvalue, size);
        if (size == 0)
            /* Mid-air collision?. Somebody must've changed the env. var. */
            return APR_INCOMPLETE;

        inchars = wcslen(wvalue) + 1;
        outchars = 3 * inchars; /* Enougn for any UTF-8 representation */
        val = fspr_palloc(pool, outchars);
        status = fspr_conv_ucs2_to_utf8(wvalue, &inchars, val, &outchars);
        if (status)
            return status;
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        char dummy;

        SetLastError(0);
        size = GetEnvironmentVariableA(envvar, &dummy, 0);
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
            /* The environment variable doesn't exist. */
            return APR_ENOENT;

        if (size == 0) {
            /* The environment value exists, but is zero-length. */
            *value = fspr_pstrdup(pool, "");
            return APR_SUCCESS;
        }

        val = fspr_palloc(pool, size);
        size = GetEnvironmentVariableA(envvar, val, size);
        if (size == 0)
            /* Mid-air collision?. Somebody must've changed the env. var. */
            return APR_INCOMPLETE;
    }
#endif

    *value = val;
    return APR_SUCCESS;
}


APR_DECLARE(fspr_status_t) fspr_env_set(const char *envvar,
                                      const char *value,
                                      fspr_pool_t *pool)
{
#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        fspr_wchar_t wenvvar[APR_PATH_MAX];
        fspr_wchar_t *wvalue;
        fspr_size_t inchars, outchars;
        fspr_status_t status;

        status = widen_envvar_name(wenvvar, APR_PATH_MAX, envvar);
        if (status)
            return status;

        outchars = inchars = strlen(value) + 1;
        wvalue = fspr_palloc(pool, outchars * sizeof(*wvalue));
        status = fspr_conv_utf8_to_ucs2(value, &inchars, wvalue, &outchars);
        if (status)
            return status;

        if (!SetEnvironmentVariableW(wenvvar, wvalue))
            return fspr_get_os_error();
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        if (!SetEnvironmentVariableA(envvar, value))
            return fspr_get_os_error();
    }
#endif

    return APR_SUCCESS;
}


APR_DECLARE(fspr_status_t) fspr_env_delete(const char *envvar, fspr_pool_t *pool)
{
#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        fspr_wchar_t wenvvar[APR_PATH_MAX];
        fspr_status_t status;

        status = widen_envvar_name(wenvvar, APR_PATH_MAX, envvar);
        if (status)
            return status;

        if (!SetEnvironmentVariableW(wenvvar, NULL))
            return fspr_get_os_error();
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        if (!SetEnvironmentVariableA(envvar, NULL))
            return fspr_get_os_error();
    }
#endif

    return APR_SUCCESS;
}
