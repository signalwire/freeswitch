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
#include "fspr_portable.h"

#if APR_HAS_DSO

static fspr_status_t dso_cleanup(void *thedso)
{
    fspr_dso_handle_t *dso = thedso;

    if (dso->handle > 0 && unload_add_on(dso->handle) < B_NO_ERROR)
        return APR_EINIT;
    dso->handle = -1;

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_dso_load(fspr_dso_handle_t **res_handle, 
                                       const char *path, fspr_pool_t *pool)
{
    image_id newid = -1;

    *res_handle = fspr_pcalloc(pool, sizeof(*res_handle));

    if((newid = load_add_on(path)) < B_NO_ERROR) {
        (*res_handle)->errormsg = strerror(newid);
        return APR_EDSOOPEN;
	}
	
    (*res_handle)->pool = pool;
    (*res_handle)->handle = newid;

    fspr_pool_cleanup_register(pool, *res_handle, dso_cleanup, fspr_pool_cleanup_null);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_dso_unload(fspr_dso_handle_t *handle)
{
    return fspr_pool_cleanup_run(handle->pool, handle, dso_cleanup);
}

APR_DECLARE(fspr_status_t) fspr_dso_sym(fspr_dso_handle_sym_t *ressym, fspr_dso_handle_t *handle,
               const char *symname)
{
    int err;

    if (symname == NULL)
        return APR_ESYMNOTFOUND;

    err = get_image_symbol(handle->handle, symname, B_SYMBOL_TYPE_ANY, 
			 ressym);

    if(err != B_OK)
        return APR_ESYMNOTFOUND;

    return APR_SUCCESS;
}

APR_DECLARE(const char *) fspr_dso_error(fspr_dso_handle_t *dso, char *buffer, fspr_size_t buflen)
{
    strncpy(buffer, strerror(errno), buflen);
    return buffer;
}

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

#endif
