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

#include "fspr_portable.h"
#include "fspr_strings.h"
#include "fspr_arch_dso.h"
#include <errno.h>
#include <string.h>

#if APR_HAS_DSO

APR_DECLARE(fspr_status_t) fspr_os_dso_handle_put(fspr_dso_handle_t **aprdso,
                                                fspr_os_dso_handle_t osdso,
                                                fspr_pool_t *pool)
{   
    *aprdso = fspr_pcalloc(pool, sizeof **aprdso);
    (*aprdso)->handle = osdso;
    (*aprdso)->pool = pool;
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
    int rc;

    if (dso->handle == 0)
        return APR_SUCCESS;
       
    rc = dllfree(dso->handle);

    if (rc == 0) {
        dso->handle = 0;
        return APR_SUCCESS;
    }
    dso->failing_errno = errno;
    return errno;
}

APR_DECLARE(fspr_status_t) fspr_dso_load(fspr_dso_handle_t **res_handle, 
                                       const char *path, fspr_pool_t *ctx)
{
    dllhandle *handle;
    int rc;

    *res_handle = fspr_pcalloc(ctx, sizeof(*res_handle));
    (*res_handle)->pool = ctx;
    if ((handle = dllload(path)) != NULL) {
        (*res_handle)->handle  = handle;
        fspr_pool_cleanup_register(ctx, *res_handle, dso_cleanup, fspr_pool_cleanup_null);
        return APR_SUCCESS;
    }

    (*res_handle)->failing_errno = errno;
    return errno;
}

APR_DECLARE(fspr_status_t) fspr_dso_unload(fspr_dso_handle_t *handle)
{
    return fspr_pool_cleanup_run(handle->pool, handle, dso_cleanup);
}

APR_DECLARE(fspr_status_t) fspr_dso_sym(fspr_dso_handle_sym_t *ressym, 
                                      fspr_dso_handle_t *handle, 
                                      const char *symname)
{
    void *func_ptr;
    void *var_ptr; 

    if ((var_ptr = dllqueryvar(handle->handle, symname)) != NULL) {
        *ressym = var_ptr;
        return APR_SUCCESS;
    }
    if ((func_ptr = (void *)dllqueryfn(handle->handle, symname)) != NULL) {
        *ressym = func_ptr;
        return APR_SUCCESS;
    }
    handle->failing_errno = errno;
    return errno;
}

APR_DECLARE(const char *) fspr_dso_error(fspr_dso_handle_t *handle, char *buffer, 
                          fspr_size_t buflen)
{
    fspr_cpystrn(buffer, strerror(handle->failing_errno), buflen);
    return buffer;
}

#endif
