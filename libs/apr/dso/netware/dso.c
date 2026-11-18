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

#include <library.h>
#include <unistd.h>

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
    sym_list *symbol = NULL;
    void *NLMHandle = getnlmhandle();

    if (dso->handle == NULL)
        return APR_SUCCESS;

    if (dso->symbols != NULL) {
        symbol = dso->symbols;
        while (symbol) {
            UnImportPublicObject(NLMHandle, symbol->symbol);
            symbol = symbol->next;
        }
    }

    if (dlclose(dso->handle) != 0)
        return APR_EINIT;

    dso->handle = NULL;
    dso->symbols = NULL;
    dso->path = NULL;

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_dso_load(fspr_dso_handle_t **res_handle, 
                                       const char *path, fspr_pool_t *pool)
{

    void *os_handle = NULL;
    char *fullpath = NULL;
    fspr_status_t rv;

    if ((rv = fspr_filepath_merge(&fullpath, NULL, path, 
                                 APR_FILEPATH_NATIVE, pool)) != APR_SUCCESS) {
        return rv;
    }

    os_handle = dlopen(fullpath, RTLD_NOW | RTLD_LOCAL);

    *res_handle = fspr_pcalloc(pool, sizeof(**res_handle));

    if(os_handle == NULL) {
        (*res_handle)->errormsg = dlerror();
        return APR_EDSOOPEN;
    }

    (*res_handle)->handle = (void*)os_handle;
    (*res_handle)->pool = pool;
    (*res_handle)->errormsg = NULL;
    (*res_handle)->symbols = NULL;
    (*res_handle)->path = fspr_pstrdup(pool, fullpath);

    fspr_pool_cleanup_register(pool, *res_handle, dso_cleanup, fspr_pool_cleanup_null);

    return APR_SUCCESS;
}
    
APR_DECLARE(fspr_status_t) fspr_dso_unload(fspr_dso_handle_t *handle)
{
    return fspr_pool_cleanup_run(handle->pool, handle, dso_cleanup);
}

APR_DECLARE(fspr_status_t) fspr_dso_sym(fspr_dso_handle_sym_t *ressym, 
                                      fspr_dso_handle_t *handle, 
                                      const char *symname)
{
    sym_list *symbol = NULL;
    void *retval = dlsym(handle->handle, symname);

    if (retval == NULL) {
        handle->errormsg = dlerror();
        return APR_ESYMNOTFOUND;
    }

    symbol = fspr_pcalloc(handle->pool, sizeof(sym_list));
    symbol->next = handle->symbols;
    handle->symbols = symbol;
    symbol->symbol = fspr_pstrdup(handle->pool, symname);

    *ressym = retval;
    
    return APR_SUCCESS;
}

APR_DECLARE(const char *) fspr_dso_error(fspr_dso_handle_t *dso, char *buffer, 
                                        fspr_size_t buflen)
{
    if (dso->errormsg) {
        fspr_cpystrn(buffer, dso->errormsg, buflen);
        return dso->errormsg;
    }
    return "No Error";
}

