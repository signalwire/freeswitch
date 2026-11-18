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

#include "win32/fspr_arch_threadproc.h"
#include "fspr_thread_proc.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_errno.h"
#include "fspr_portable.h"

APR_DECLARE(fspr_status_t) fspr_threadkey_private_create(fspr_threadkey_t **key,
                                                    void (*dest)(void *),
                                                    fspr_pool_t *pool)
{
    (*key) = (fspr_threadkey_t *)fspr_palloc(pool, sizeof(fspr_threadkey_t));
    if ((*key) == NULL) {
        return APR_ENOMEM;
    }

    (*key)->pool = pool;

    if (((*key)->key = TlsAlloc()) != 0xFFFFFFFF) {
        return APR_SUCCESS;
    }
    return fspr_get_os_error();
}

APR_DECLARE(fspr_status_t) fspr_threadkey_private_get(void **new,
                                                 fspr_threadkey_t *key)
{
    if ((*new) = TlsGetValue(key->key)) {
        return APR_SUCCESS;
    }
    return fspr_get_os_error();
}

APR_DECLARE(fspr_status_t) fspr_threadkey_private_set(void *priv,
                                                 fspr_threadkey_t *key)
{
    if (TlsSetValue(key->key, priv)) {
        return APR_SUCCESS;
    }
    return fspr_get_os_error();
}

APR_DECLARE(fspr_status_t) fspr_threadkey_private_delete(fspr_threadkey_t *key)
{
    if (TlsFree(key->key)) {
        return APR_SUCCESS; 
    }
    return fspr_get_os_error();
}

APR_DECLARE(fspr_status_t) fspr_threadkey_data_get(void **data, const char *key,
                                                fspr_threadkey_t *threadkey)
{
    return fspr_pool_userdata_get(data, key, threadkey->pool);
}

APR_DECLARE(fspr_status_t) fspr_threadkey_data_set(void *data, const char *key,
                                                fspr_status_t (*cleanup)(void *),
                                                fspr_threadkey_t *threadkey)
{
    return fspr_pool_userdata_set(data, key, cleanup, threadkey->pool);
}

APR_DECLARE(fspr_status_t) fspr_os_threadkey_get(fspr_os_threadkey_t *thekey,
                                               fspr_threadkey_t *key)
{
    *thekey = key->key;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_threadkey_put(fspr_threadkey_t **key,
                                               fspr_os_threadkey_t *thekey,
                                               fspr_pool_t *pool)
{
    if (pool == NULL) {
        return APR_ENOPOOL;
    }
    if ((*key) == NULL) {
        (*key) = (fspr_threadkey_t *)fspr_palloc(pool, sizeof(fspr_threadkey_t));
        (*key)->pool = pool;
    }
    (*key)->key = *thekey;
    return APR_SUCCESS;
}           

