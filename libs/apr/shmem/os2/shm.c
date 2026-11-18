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

#include "fspr_general.h"
#include "fspr_shm.h"
#include "fspr_errno.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "fspr_portable.h"

struct fspr_shm_t {
    fspr_pool_t *pool;
    void *memblock;
};

APR_DECLARE(fspr_status_t) fspr_shm_create(fspr_shm_t **m,
                                         fspr_size_t reqsize,
                                         const char *filename,
                                         fspr_pool_t *pool)
{
    int rc;
    fspr_shm_t *newm = (fspr_shm_t *)fspr_palloc(pool, sizeof(fspr_shm_t));
    char *name = NULL;
    ULONG flags = PAG_COMMIT|PAG_READ|PAG_WRITE;

    newm->pool = pool;

    if (filename) {
        name = fspr_pstrcat(pool, "\\SHAREMEM\\", filename, NULL);
    }

    if (name == NULL) {
        flags |= OBJ_GETTABLE;
    }

    rc = DosAllocSharedMem(&(newm->memblock), name, reqsize, flags);

    if (rc) {
        return APR_OS2_STATUS(rc);
    }

    *m = newm;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_shm_destroy(fspr_shm_t *m)
{
    DosFreeMem(m->memblock);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_shm_remove(const char *filename,
                                         fspr_pool_t *pool)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_shm_attach(fspr_shm_t **m,
                                         const char *filename,
                                         fspr_pool_t *pool)
{
    int rc;
    fspr_shm_t *newm = (fspr_shm_t *)fspr_palloc(pool, sizeof(fspr_shm_t));
    char *name = NULL;
    ULONG flags = PAG_READ|PAG_WRITE;

    newm->pool = pool;
    name = fspr_pstrcat(pool, "\\SHAREMEM\\", filename, NULL);

    rc = DosGetNamedSharedMem(&(newm->memblock), name, flags);

    if (rc) {
        return APR_FROM_OS_ERROR(rc);
    }

    *m = newm;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_shm_detach(fspr_shm_t *m)
{
    int rc = 0;

    if (m->memblock) {
        rc = DosFreeMem(m->memblock);
    }

    return APR_FROM_OS_ERROR(rc);
}

APR_DECLARE(void *) fspr_shm_baseaddr_get(const fspr_shm_t *m)
{
    return m->memblock;
}

APR_DECLARE(fspr_size_t) fspr_shm_size_get(const fspr_shm_t *m)
{
    ULONG flags, size = 0x1000000;
    DosQueryMem(m->memblock, &size, &flags);
    return size;
}

APR_POOL_IMPLEMENT_ACCESSOR(shm)

APR_DECLARE(fspr_status_t) fspr_os_shm_get(fspr_os_shm_t *osshm,
                                         fspr_shm_t *shm)
{
    *osshm = shm->memblock;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_shm_put(fspr_shm_t **m,
                                         fspr_os_shm_t *osshm,
                                         fspr_pool_t *pool)
{
    int rc;
    fspr_shm_t *newm = (fspr_shm_t *)fspr_palloc(pool, sizeof(fspr_shm_t));
    ULONG flags = PAG_COMMIT|PAG_READ|PAG_WRITE;

    newm->pool = pool;

    rc = DosGetSharedMem(&(newm->memblock), flags);

    if (rc) {
        return APR_FROM_OS_ERROR(rc);
    }

    *m = newm;
    return APR_SUCCESS;
}    

