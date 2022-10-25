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

#include "fspr_arch_file_io.h"
#include "fspr_file_io.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "fspr_portable.h"
#include <string.h>

static fspr_status_t dir_cleanup(void *thedir)
{
    fspr_dir_t *dir = thedir;
    return fspr_dir_close(dir);
}



APR_DECLARE(fspr_status_t) fspr_dir_open(fspr_dir_t **new, const char *dirname, fspr_pool_t *pool)
{
    fspr_dir_t *thedir = (fspr_dir_t *)fspr_palloc(pool, sizeof(fspr_dir_t));
    
    if (thedir == NULL)
        return APR_ENOMEM;
    
    thedir->pool = pool;
    thedir->dirname = fspr_pstrdup(pool, dirname);

    if (thedir->dirname == NULL)
        return APR_ENOMEM;

    thedir->handle = 0;
    thedir->validentry = FALSE;
    *new = thedir;
    fspr_pool_cleanup_register(pool, thedir, dir_cleanup, fspr_pool_cleanup_null);
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_dir_close(fspr_dir_t *thedir)
{
    int rv = 0;
    
    if (thedir->handle) {
        rv = DosFindClose(thedir->handle);
        
        if (rv == 0) {
            thedir->handle = 0;
        }
    }

    return APR_FROM_OS_ERROR(rv);
} 



APR_DECLARE(fspr_status_t) fspr_dir_read(fspr_finfo_t *finfo, fspr_int32_t wanted,
                                       fspr_dir_t *thedir)
{
    int rv;
    ULONG entries = 1;
    
    if (thedir->handle == 0) {
        thedir->handle = HDIR_CREATE;
        rv = DosFindFirst(fspr_pstrcat(thedir->pool, thedir->dirname, "/*", NULL), &thedir->handle, 
                          FILE_ARCHIVED|FILE_DIRECTORY|FILE_SYSTEM|FILE_HIDDEN|FILE_READONLY, 
                          &thedir->entry, sizeof(thedir->entry), &entries, FIL_STANDARD);
    } else {
        rv = DosFindNext(thedir->handle, &thedir->entry, sizeof(thedir->entry), &entries);
    }

    finfo->pool = thedir->pool;
    finfo->fname = NULL;
    finfo->valid = 0;

    if (rv == 0 && entries == 1) {
        thedir->validentry = TRUE;

        /* We passed a name off the stack that has popped */
        finfo->fname = NULL;
        finfo->size = thedir->entry.cbFile;
        finfo->csize = thedir->entry.cbFileAlloc;

        /* Only directories & regular files show up in directory listings */
        finfo->filetype = (thedir->entry.attrFile & FILE_DIRECTORY) ? APR_DIR : APR_REG;

        fspr_os2_time_to_fspr_time(&finfo->mtime, thedir->entry.fdateLastWrite,
                                 thedir->entry.ftimeLastWrite);
        fspr_os2_time_to_fspr_time(&finfo->atime, thedir->entry.fdateLastAccess,
                                 thedir->entry.ftimeLastAccess);
        fspr_os2_time_to_fspr_time(&finfo->ctime, thedir->entry.fdateCreation,
                                 thedir->entry.ftimeCreation);

        finfo->name = thedir->entry.achName;
        finfo->valid = APR_FINFO_NAME | APR_FINFO_MTIME | APR_FINFO_ATIME |
            APR_FINFO_CTIME | APR_FINFO_TYPE | APR_FINFO_SIZE |
            APR_FINFO_CSIZE;

        return APR_SUCCESS;
    }

    thedir->validentry = FALSE;

    if (rv)
        return APR_FROM_OS_ERROR(rv);

    return APR_ENOENT;
}



APR_DECLARE(fspr_status_t) fspr_dir_rewind(fspr_dir_t *thedir)
{
    return fspr_dir_close(thedir);
}



APR_DECLARE(fspr_status_t) fspr_dir_make(const char *path, fspr_fileperms_t perm, fspr_pool_t *pool)
{
    return APR_FROM_OS_ERROR(DosCreateDir(path, NULL));
}



APR_DECLARE(fspr_status_t) fspr_dir_remove(const char *path, fspr_pool_t *pool)
{
    return APR_FROM_OS_ERROR(DosDeleteDir(path));
}



APR_DECLARE(fspr_status_t) fspr_os_dir_get(fspr_os_dir_t **thedir, fspr_dir_t *dir)
{
    if (dir == NULL) {
        return APR_ENODIR;
    }
    *thedir = &dir->handle;
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_os_dir_put(fspr_dir_t **dir, fspr_os_dir_t *thedir,
                                         fspr_pool_t *pool)
{
    if ((*dir) == NULL) {
        (*dir) = (fspr_dir_t *)fspr_pcalloc(pool, sizeof(fspr_dir_t));
        (*dir)->pool = pool;
    }
    (*dir)->handle = *thedir;
    return APR_SUCCESS;
}
