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
#include "fspr_portable.h"
#include <stdio.h>
#include <string.h>

#if APR_HAS_DSO

static fspr_status_t dso_cleanup(void *thedso)
{
    fspr_dso_handle_t *dso = thedso;
    int rc;

    if (dso->handle == 0)
        return APR_SUCCESS;
       
    rc = DosFreeModule(dso->handle);

    if (rc == 0)
        dso->handle = 0;

    return APR_FROM_OS_ERROR(rc);
}


APR_DECLARE(fspr_status_t) fspr_dso_load(fspr_dso_handle_t **res_handle, const char *path, fspr_pool_t *ctx)
{
    char failed_module[200];
    HMODULE handle;
    int rc;

    *res_handle = fspr_pcalloc(ctx, sizeof(**res_handle));
    (*res_handle)->cont = ctx;
    (*res_handle)->load_error = APR_SUCCESS;
    (*res_handle)->failed_module = NULL;

    if ((rc = DosLoadModule(failed_module, sizeof(failed_module), path, &handle)) != 0) {
        (*res_handle)->load_error = APR_FROM_OS_ERROR(rc);
        (*res_handle)->failed_module = fspr_pstrdup(ctx, failed_module);
        return APR_FROM_OS_ERROR(rc);
    }

    (*res_handle)->handle  = handle;
    fspr_pool_cleanup_register(ctx, *res_handle, dso_cleanup, fspr_pool_cleanup_null);
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_dso_unload(fspr_dso_handle_t *handle)
{
    return fspr_pool_cleanup_run(handle->cont, handle, dso_cleanup);
}



APR_DECLARE(fspr_status_t) fspr_dso_sym(fspr_dso_handle_sym_t *ressym, 
                                      fspr_dso_handle_t *handle, 
                                      const char *symname)
{
    PFN func;
    int rc;

    if (symname == NULL || ressym == NULL)
        return APR_ESYMNOTFOUND;

    if ((rc = DosQueryProcAddr(handle->handle, 0, symname, &func)) != 0) {
        handle->load_error = APR_FROM_OS_ERROR(rc);
        return handle->load_error;
    }

    *ressym = func;
    return APR_SUCCESS;
}



APR_DECLARE(const char *) fspr_dso_error(fspr_dso_handle_t *dso, char *buffer, fspr_size_t buflen)
{
    char message[200];
    fspr_strerror(dso->load_error, message, sizeof(message));

    if (dso->failed_module != NULL) {
        strcat(message, " (");
        strcat(message, dso->failed_module);
        strcat(message, ")");
    }

    fspr_cpystrn(buffer, message, buflen);
    return buffer;
}



APR_DECLARE(fspr_status_t) fspr_os_dso_handle_put(fspr_dso_handle_t **aprdso,
                                                fspr_os_dso_handle_t osdso,
                                                fspr_pool_t *pool)
{
    *aprdso = fspr_pcalloc(pool, sizeof **aprdso);
    (*aprdso)->handle = osdso;
    (*aprdso)->cont = pool;
    (*aprdso)->load_error = APR_SUCCESS;
    (*aprdso)->failed_module = NULL;
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_os_dso_handle_get(fspr_os_dso_handle_t *osdso,
                                                fspr_dso_handle_t *aprdso)
{
    *osdso = aprdso->handle;
    return APR_SUCCESS;
}

#endif
