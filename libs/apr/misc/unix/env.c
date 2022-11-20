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
#include "fspr_private.h"
#include "fspr_env.h"

#if APR_HAVE_UNISTD_H
#include <unistd.h>
#endif
#if APR_HAVE_STDLIB_H
#include <stdlib.h>
#endif

APR_DECLARE(fspr_status_t) fspr_env_get(char **value,
                                      const char *envvar,
                                      fspr_pool_t *pool)
{
#ifdef HAVE_GETENV

    char *val = getenv(envvar);
    if (!val)
        return APR_ENOENT;
    *value = val;
    return APR_SUCCESS;

#else
    return APR_ENOTIMPL;
#endif
}


APR_DECLARE(fspr_status_t) fspr_env_set(const char *envvar,
                                      const char *value,
                                      fspr_pool_t *pool)
{
#if defined(HAVE_SETENV)

    if (0 > setenv(envvar, value, 1))
        return APR_ENOMEM;
    return APR_SUCCESS;

#elif defined(HAVE_PUTENV)

    fspr_size_t elen = strlen(envvar);
    fspr_size_t vlen = strlen(value);
    char *env = fspr_palloc(pool, elen + vlen + 2);
    char *p = env + elen;

    memcpy(env, envvar, elen);
    *p++ = '=';
    memcpy(p, value, vlen);
    p[vlen] = '\0';

    if (0 > putenv(env))
        return APR_ENOMEM;
    return APR_SUCCESS;

#else
    return APR_ENOTIMPL;
#endif
}


APR_DECLARE(fspr_status_t) fspr_env_delete(const char *envvar, fspr_pool_t *pool)
{
#ifdef HAVE_UNSETENV

    unsetenv(envvar);
    return APR_SUCCESS;

#else
    /* hint: some platforms allow envvars to be unset via
     *       putenv("varname")...  that isn't Single Unix spec,
     *       but if your platform doesn't have unsetenv() it is
     *       worth investigating and potentially adding a
     *       configure check to decide when to use that form of
     *       putenv() here
     */
    return APR_ENOTIMPL;
#endif
}
