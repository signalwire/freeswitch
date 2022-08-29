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

#include "fspr.h"
#include "fspr_private.h"
#include "fspr_general.h"
#include "fspr_mmap.h"
#include "fspr_errno.h"
#include "fspr_arch_file_io.h"
#include "fspr_portable.h"
#include "fspr_strings.h"

#if APR_HAS_MMAP

static fspr_status_t mmap_cleanup(void *themmap)
{
    fspr_mmap_t *mm = themmap;
    fspr_mmap_t *next = APR_RING_NEXT(mm,link);
    fspr_status_t rv = 0;

    /* we no longer refer to the mmaped region */
    APR_RING_REMOVE(mm,link);
    APR_RING_NEXT(mm,link) = NULL;
    APR_RING_PREV(mm,link) = NULL;

    if (next != mm) {
        /* more references exist, so we're done */
        return APR_SUCCESS;
    }

    if (mm->mv) {
        if (!UnmapViewOfFile(mm->mv))
        {
            fspr_status_t rv = fspr_get_os_error();
            CloseHandle(mm->mhandle);
            mm->mv = NULL;
            mm->mhandle = NULL;
            return rv;
        }
        mm->mv = NULL;
    }
    if (mm->mhandle) 
    {
        if (!CloseHandle(mm->mhandle))
        {
            fspr_status_t rv = fspr_get_os_error();
            CloseHandle(mm->mhandle);
            mm->mhandle = NULL;
            return rv;
        }
        mm->mhandle = NULL;
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_mmap_create(fspr_mmap_t **new, fspr_file_t *file,
                                          fspr_off_t offset, fspr_size_t size,
                                          fspr_int32_t flag, fspr_pool_t *cont)
{
    static DWORD memblock = 0;
    DWORD fmaccess = 0;
    DWORD mvaccess = 0;
    DWORD offlo;
    DWORD offhi;

    if (size == 0)
        return APR_EINVAL;
    
    if (flag & APR_MMAP_WRITE)
        fmaccess |= PAGE_READWRITE;
    else if (flag & APR_MMAP_READ)
        fmaccess |= PAGE_READONLY;

    if (flag & APR_MMAP_READ)
        mvaccess |= FILE_MAP_READ;
    if (flag & APR_MMAP_WRITE)
        mvaccess |= FILE_MAP_WRITE;

    if (!file || !file->filehand || file->filehand == INVALID_HANDLE_VALUE
        || file->buffered)
        return APR_EBADF;

    if (!memblock)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        memblock = si.dwAllocationGranularity;
    }   
    
    *new = fspr_pcalloc(cont, sizeof(fspr_mmap_t));
    (*new)->pstart = (offset / memblock) * memblock;
    (*new)->poffset = offset - (*new)->pstart;
    (*new)->psize = (fspr_size_t)((*new)->poffset) + size;
    /* The size of the CreateFileMapping object is the current size
     * of the size of the mmap object (e.g. file size), not the size 
     * of the mapped region!
     */

    (*new)->mhandle = CreateFileMapping(file->filehand, NULL, fmaccess,
                                        0, 0, NULL);
    if (!(*new)->mhandle || (*new)->mhandle == INVALID_HANDLE_VALUE)
    {
        *new = NULL;
        return fspr_get_os_error();
    }

    offlo = (DWORD)(*new)->pstart;
    offhi = (DWORD)((*new)->pstart >> 32);
    (*new)->mv = MapViewOfFile((*new)->mhandle, mvaccess, offhi,
                               offlo, (*new)->psize);
    if (!(*new)->mv)
    {
        fspr_status_t rv = fspr_get_os_error();
        CloseHandle((*new)->mhandle);
        *new = NULL;
        return rv;
    }

    (*new)->mm = (char*)((*new)->mv) + (*new)->poffset;
    (*new)->size = size;
    (*new)->cntxt = cont;
    APR_RING_ELEM_INIT(*new, link);

    /* register the cleanup... */
    fspr_pool_cleanup_register((*new)->cntxt, (void*)(*new), mmap_cleanup,
                         fspr_pool_cleanup_null);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_mmap_dup(fspr_mmap_t **new_mmap,
                                       fspr_mmap_t *old_mmap,
                                       fspr_pool_t *p)
{
    *new_mmap = (fspr_mmap_t *)fspr_pmemdup(p, old_mmap, sizeof(fspr_mmap_t));
    (*new_mmap)->cntxt = p;

    APR_RING_INSERT_AFTER(old_mmap, *new_mmap, link);

    fspr_pool_cleanup_register(p, *new_mmap, mmap_cleanup,
                              fspr_pool_cleanup_null);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_mmap_delete(fspr_mmap_t *mm)
{
    return fspr_pool_cleanup_run(mm->cntxt, mm, mmap_cleanup);
}

#endif
