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

#include "apr_general.h"
#include "apr_errno.h"
#include "apr_file_io.h"
#include "apr_shm.h"
#include "apr_arch_file_io.h"
#include "limits.h"

typedef struct memblock_t {
    apr_size_t size;
    apr_size_t length;
} memblock_t;

struct apr_shm_t {
    apr_pool_t *pool;
    memblock_t *memblk;
    void       *usrmem;
    apr_size_t  size;
    apr_size_t  length;
    HANDLE      hMap;
};

static apr_status_t shm_cleanup(void* shm)
{
    apr_status_t rv = APR_SUCCESS;
    apr_shm_t *m = shm;
    
    if (UnmapViewOfFile(m->memblk)) {
        rv = apr_get_os_error();
    }
    if (CloseHandle(m->hMap)) {
        return (rv != APR_SUCCESS) ? rv : apr_get_os_error();
    }
    /* ### Do we want to make a point of unlinking m->file here? 
     * Need to add the fname to the apr_shm_t, in that case.
     */
    return rv;
}

APR_DECLARE(apr_status_t) apr_shm_create(apr_shm_t **m,
                                         apr_size_t reqsize,
                                         const char *file,
                                         apr_pool_t *pool)
{
    static apr_size_t memblock = 0;
    HANDLE hMap, hFile;
    apr_status_t rv;
    apr_size_t size;
    apr_file_t *f;
    void *base;
    void *mapkey;
    DWORD err, sizelo, sizehi;

    reqsize += sizeof(memblock_t);

    if (!memblock)
    {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        memblock = si.dwAllocationGranularity;
    }   

    /* Compute the granualar multiple of the pagesize */
    size = memblock * (1 + (reqsize - 1) / memblock);
    sizelo = (DWORD)size;
#ifdef WIN64
    sizehi = (DWORD)(size >> 32);
#else
    sizehi = 0;
#endif

    if (!file) {
        /* Do Anonymous, which must be passed as a duplicated handle */
#ifndef _WIN32_WCE
        hFile = INVALID_HANDLE_VALUE;
#endif
        mapkey = NULL;
    }
    else {
        /* Do file backed, which is not an inherited handle 
         * While we could open APR_EXCL, it doesn't seem that Unix
         * ever did.  Ignore that error here, but fail later when
         * we discover we aren't the creator of the file map object.
         */
        rv = apr_file_open(&f, file,
                           APR_READ | APR_WRITE | APR_BINARY | APR_CREATE,
                           APR_UREAD | APR_UWRITE, pool);
        if ((rv != APR_SUCCESS)
                || ((rv = apr_os_file_get(&hFile, f)) != APR_SUCCESS)) {
            return rv;
        }
        rv = apr_file_trunc(f, size);

        /* res_name_from_filename turns file into a pseudo-name
         * without slashes or backslashes, and prepends the \global
         * prefix on Win2K and later
         */
        mapkey = res_name_from_filename(file, 1, pool);
    }

#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
        hMap = CreateFileMappingW(hFile, NULL, PAGE_READWRITE, 
                                  sizehi, sizelo, mapkey);
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        hMap = CreateFileMappingA(hFile, NULL, PAGE_READWRITE, 
                                  sizehi, sizelo, mapkey);
    }
#endif
    err = apr_get_os_error();

    if (file) {
        apr_file_close(f);
    }

    if (hMap && err == ERROR_ALREADY_EXISTS) {
        CloseHandle(hMap);
        return APR_EEXIST;
    }
    if (!hMap) {
        return err;
    }
    
    base = MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE,
                         0, 0, size);
    if (!base) {
        CloseHandle(hMap);
        return apr_get_os_error();
    }
    
    *m = (apr_shm_t *) apr_palloc(pool, sizeof(apr_shm_t));
    (*m)->pool = pool;
    (*m)->hMap = hMap;
    (*m)->memblk = base;
    (*m)->size = size;

    (*m)->usrmem = (char*)base + sizeof(memblock_t);
    (*m)->length = reqsize - sizeof(memblock_t);;
    
    (*m)->memblk->length = (*m)->length;
    (*m)->memblk->size = (*m)->size;

    apr_pool_cleanup_register((*m)->pool, *m, 
                              shm_cleanup, apr_pool_cleanup_null);
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_shm_destroy(apr_shm_t *m) 
{
    apr_status_t rv = shm_cleanup(m);
    apr_pool_cleanup_kill(m->pool, m, shm_cleanup);
    return rv;
}

APR_DECLARE(apr_status_t) apr_shm_remove(const char *filename,
                                         apr_pool_t *pool)
{
    return APR_ENOTIMPL;
}

APR_DECLARE(apr_status_t) apr_shm_attach(apr_shm_t **m,
                                         const char *file,
                                         apr_pool_t *pool)
{
    HANDLE hMap;
    void *mapkey;
    void *base;

    if (!file) {
        return APR_EINVAL;
    }
    else {
        /* res_name_from_filename turns file into a pseudo-name
         * without slashes or backslashes, and prepends the \global
         * prefix on Win2K and later
         */
        mapkey = res_name_from_filename(file, 1, pool);
    }

#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE
    {
#ifndef _WIN32_WCE
        hMap = OpenFileMappingW(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, mapkey);
#else
        /* The WCE 3.0 lacks OpenFileMapping. So we emulate one with
         * opening the existing shmem and reading its size from the header 
         */
        hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, 
                                  PAGE_READWRITE, 0, sizeof(apr_shm_t), mapkey);
#endif
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI
    {
        hMap = OpenFileMappingA(FILE_MAP_READ | FILE_MAP_WRITE, FALSE, mapkey);
    }
#endif

    if (!hMap) {
        return apr_get_os_error();
    }
    
    base = MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        return apr_get_os_error();
    }
    
    *m = (apr_shm_t *) apr_palloc(pool, sizeof(apr_shm_t));
    (*m)->pool = pool;
    (*m)->memblk = base;
    /* Real (*m)->mem->size could be recovered with VirtualQuery */
    (*m)->size = (*m)->memblk->size;
#if _WIN32_WCE
    /* Reopen with real size  */
    UnmapViewOfFile(base);
    CloseHandle(hMap);

    hMap = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, 
                              PAGE_READWRITE, 0, (*m)->size, mapkey);
    if (!hMap) {
        return apr_get_os_error();
    }
    base = MapViewOfFile(hMap, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (!base) {
        CloseHandle(hMap);
        return apr_get_os_error();
    }    
#endif
    (*m)->hMap = hMap;
    (*m)->length = (*m)->memblk->length;
    (*m)->usrmem = (char*)base + sizeof(memblock_t);
    apr_pool_cleanup_register((*m)->pool, *m, 
                              shm_cleanup, apr_pool_cleanup_null);
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_shm_detach(apr_shm_t *m)
{
    apr_status_t rv = shm_cleanup(m);
    apr_pool_cleanup_kill(m->pool, m, shm_cleanup);
    return rv;
}

APR_DECLARE(void *) apr_shm_baseaddr_get(const apr_shm_t *m)
{
    return m->usrmem;
}

APR_DECLARE(apr_size_t) apr_shm_size_get(const apr_shm_t *m)
{
    return m->length;
}

APR_POOL_IMPLEMENT_ACCESSOR(shm)

APR_DECLARE(apr_status_t) apr_os_shm_get(apr_os_shm_t *osshm,
                                         apr_shm_t *shm)
{
    *osshm = shm->hMap;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_os_shm_put(apr_shm_t **m,
                                         apr_os_shm_t *osshm,
                                         apr_pool_t *pool)
{
    void* base;
    base = MapViewOfFile(*osshm, FILE_MAP_READ | FILE_MAP_WRITE, 0, 0, 0);
    if (!base) {
        return apr_get_os_error();
    }
    
    *m = (apr_shm_t *) apr_palloc(pool, sizeof(apr_shm_t));
    (*m)->pool = pool;
    (*m)->hMap = *osshm;
    (*m)->memblk = base;
    (*m)->usrmem = (char*)base + sizeof(memblock_t);
    /* Real (*m)->mem->size could be recovered with VirtualQuery */
    (*m)->size = (*m)->memblk->size;
    (*m)->length = (*m)->memblk->length;

    apr_pool_cleanup_register((*m)->pool, *m, 
                              shm_cleanup, apr_pool_cleanup_null);
    return APR_SUCCESS;
}    

