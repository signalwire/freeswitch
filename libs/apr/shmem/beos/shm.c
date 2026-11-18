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
#include <stdio.h>
#include <stdlib.h>
#include <kernel/OS.h>
#include "fspr_portable.h"

struct fspr_shm_t {
    fspr_pool_t *pool;
    void *memblock;
    void *ptr;
    fspr_size_t reqsize;
    fspr_size_t avail;
    area_id aid;
};

APR_DECLARE(fspr_status_t) fspr_shm_create(fspr_shm_t **m, 
                                         fspr_size_t reqsize, 
                                         const char *filename, 
                                         fspr_pool_t *p)
{
    fspr_size_t pagesize;
    area_id newid;
    char *addr;
    char shname[B_OS_NAME_LENGTH];
    
    (*m) = (fspr_shm_t *)fspr_pcalloc(p, sizeof(fspr_shm_t));
    /* we MUST allocate in pages, so calculate how big an area we need... */
    pagesize = ((reqsize + B_PAGE_SIZE - 1) / B_PAGE_SIZE) * B_PAGE_SIZE;
     
    if (!filename) {
        int num = 0;
        snprintf(shname, B_OS_NAME_LENGTH, "fspr_shmem_%ld", find_thread(NULL));
        while (find_area(shname) >= 0)
            snprintf(shname, B_OS_NAME_LENGTH, "fspr_shmem_%ld_%d",
                     find_thread(NULL), num++);
    }
    newid = create_area(filename ? filename : shname, 
                        (void*)&addr, B_ANY_ADDRESS,
                        pagesize, B_LAZY_LOCK, B_READ_AREA|B_WRITE_AREA);

    if (newid < 0)
        return errno;

    (*m)->pool = p;
    (*m)->aid = newid;
    (*m)->memblock = addr;
    (*m)->ptr = (void*)addr;
    (*m)->avail = pagesize; /* record how big an area we actually created... */
    (*m)->reqsize = reqsize;

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_shm_destroy(fspr_shm_t *m)
{
    delete_area(m->aid);
    m->avail = 0;
    m->memblock = NULL;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_shm_remove(const char *filename,
                                         fspr_pool_t *pool)
{
    area_id deleteme = find_area(filename);
    
    if (deleteme == B_NAME_NOT_FOUND)
        return APR_EINVAL;

    delete_area(deleteme);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_shm_attach(fspr_shm_t **m,
                                         const char *filename,
                                         fspr_pool_t *pool)
{
    area_info ai;
    thread_info ti;
    fspr_shm_t *new_m;
    area_id deleteme = find_area(filename);

    if (deleteme == B_NAME_NOT_FOUND)
        return APR_EINVAL;

    new_m = (fspr_shm_t*)fspr_palloc(pool, sizeof(fspr_shm_t*));
    if (new_m == NULL)
        return APR_ENOMEM;
    new_m->pool = pool;

    get_area_info(deleteme, &ai);
    get_thread_info(find_thread(NULL), &ti);

    if (ti.team != ai.team) {
        area_id narea;
        
        narea = clone_area(ai.name, &(ai.address), B_CLONE_ADDRESS,
                           B_READ_AREA|B_WRITE_AREA, ai.area);

        if (narea < B_OK)
            return narea;
            
        get_area_info(narea, &ai);
        new_m->aid = narea;
        new_m->memblock = ai.address;
        new_m->ptr = (void*)ai.address;
        new_m->avail = ai.size;
        new_m->reqsize = ai.size;
    }

    (*m) = new_m;
    
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_shm_detach(fspr_shm_t *m)
{
    delete_area(m->aid);
    return APR_SUCCESS;
}

APR_DECLARE(void *) fspr_shm_baseaddr_get(const fspr_shm_t *m)
{
    return m->memblock;
}

APR_DECLARE(fspr_size_t) fspr_shm_size_get(const fspr_shm_t *m)
{
    return m->reqsize;
}

APR_POOL_IMPLEMENT_ACCESSOR(shm)

APR_DECLARE(fspr_status_t) fspr_os_shm_get(fspr_os_shm_t *osshm,
                                         fspr_shm_t *shm)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(fspr_status_t) fspr_os_shm_put(fspr_shm_t **m,
                                         fspr_os_shm_t *osshm,
                                         fspr_pool_t *pool)
{
    return APR_ENOTIMPL;
}    

