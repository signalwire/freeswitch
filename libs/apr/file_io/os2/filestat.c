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

#define INCL_DOS
#define INCL_DOSERRORS
#include "fspr_arch_file_io.h"
#include "fspr_file_io.h"
#include "fspr_lib.h"
#include <sys/time.h>
#include "fspr_strings.h"


static void FS3_to_finfo(fspr_finfo_t *finfo, FILESTATUS3 *fstatus)
{
    finfo->protection = (fstatus->attrFile & FILE_READONLY) ? 0x555 : 0x777;

    if (fstatus->attrFile & FILE_DIRECTORY)
        finfo->filetype = APR_DIR;
    else
        finfo->filetype = APR_REG;
    /* XXX: No other possible types from FS3? */

    finfo->user = 0;
    finfo->group = 0;
    finfo->inode = 0;
    finfo->device = 0;
    finfo->size = fstatus->cbFile;
    finfo->csize = fstatus->cbFileAlloc;
    fspr_os2_time_to_fspr_time(&finfo->atime, fstatus->fdateLastAccess, 
                             fstatus->ftimeLastAccess );
    fspr_os2_time_to_fspr_time(&finfo->mtime, fstatus->fdateLastWrite,  
                             fstatus->ftimeLastWrite );
    fspr_os2_time_to_fspr_time(&finfo->ctime, fstatus->fdateCreation,   
                             fstatus->ftimeCreation );
    finfo->valid = APR_FINFO_TYPE | APR_FINFO_PROT | APR_FINFO_SIZE
                 | APR_FINFO_CSIZE | APR_FINFO_MTIME 
                 | APR_FINFO_CTIME | APR_FINFO_ATIME | APR_FINFO_LINK;
}



static fspr_status_t handle_type(fspr_filetype_e *ftype, HFILE file)
{
    ULONG filetype, fileattr, rc;

    rc = DosQueryHType(file, &filetype, &fileattr);

    if (rc == 0) {
        switch (filetype & 0xff) {
        case 0:
            *ftype = APR_REG;
            break;

        case 1:
            *ftype = APR_CHR;
            break;

        case 2:
            *ftype = APR_PIPE;
            break;

        default:
            /* Brian, is this correct???
             */
            *ftype = APR_UNKFILE;
            break;
        }

        return APR_SUCCESS;
    }
    return APR_FROM_OS_ERROR(rc);
}



APR_DECLARE(fspr_status_t) fspr_file_info_get(fspr_finfo_t *finfo, fspr_int32_t wanted, 
                                   fspr_file_t *thefile)
{
    ULONG rc;
    FILESTATUS3 fstatus;

    if (thefile->isopen) {
        if (thefile->buffered) {
            fspr_status_t rv = fspr_file_flush(thefile);

            if (rv != APR_SUCCESS) {
                return rv;
            }
        }

        rc = DosQueryFileInfo(thefile->filedes, FIL_STANDARD, &fstatus, sizeof(fstatus));
    }
    else
        rc = DosQueryPathInfo(thefile->fname, FIL_STANDARD, &fstatus, sizeof(fstatus));

    if (rc == 0) {
        FS3_to_finfo(finfo, &fstatus);
        finfo->fname = thefile->fname;

        if (finfo->filetype == APR_REG) {
            if (thefile->isopen) {
                return handle_type(&finfo->filetype, thefile->filedes);
            }
        } else {
            return APR_SUCCESS;
        }
    }

    finfo->protection = 0;
    finfo->filetype = APR_NOFILE;
    return APR_FROM_OS_ERROR(rc);
}

APR_DECLARE(fspr_status_t) fspr_file_perms_set(const char *fname, fspr_fileperms_t perms)
{
    return APR_ENOTIMPL;
}


APR_DECLARE(fspr_status_t) fspr_stat(fspr_finfo_t *finfo, const char *fname,
                              fspr_int32_t wanted, fspr_pool_t *cont)
{
    ULONG rc;
    FILESTATUS3 fstatus;
    
    finfo->protection = 0;
    finfo->filetype = APR_NOFILE;
    finfo->name = NULL;
    rc = DosQueryPathInfo(fname, FIL_STANDARD, &fstatus, sizeof(fstatus));
    
    if (rc == 0) {
        FS3_to_finfo(finfo, &fstatus);
        finfo->fname = fname;

        if (wanted & APR_FINFO_NAME) {
            ULONG count = 1;
            HDIR hDir = HDIR_SYSTEM;
            FILEFINDBUF3 ffb;
            rc = DosFindFirst(fname, &hDir,
                              FILE_DIRECTORY|FILE_HIDDEN|FILE_SYSTEM|FILE_ARCHIVED,
                              &ffb, sizeof(ffb), &count, FIL_STANDARD);
            if (rc == 0 && count == 1) {
                finfo->name = fspr_pstrdup(cont, ffb.achName);
                finfo->valid |= APR_FINFO_NAME;
            }
        }
    } else if (rc == ERROR_INVALID_ACCESS) {
        memset(finfo, 0, sizeof(fspr_finfo_t));
        finfo->valid = APR_FINFO_TYPE | APR_FINFO_PROT;
        finfo->protection = 0666;
        finfo->filetype = APR_CHR;

        if (wanted & APR_FINFO_NAME) {
            finfo->name = fspr_pstrdup(cont, fname);
            finfo->valid |= APR_FINFO_NAME;
        }
    } else {
        return APR_FROM_OS_ERROR(rc);
    }

    return (wanted & ~finfo->valid) ? APR_INCOMPLETE : APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_file_attrs_set(const char *fname,
                                             fspr_fileattrs_t attributes,
                                             fspr_fileattrs_t attr_mask,
                                             fspr_pool_t *cont)
{
    FILESTATUS3 fs3;
    ULONG rc;

    /* Don't do anything if we can't handle the requested attributes */
    if (!(attr_mask & (APR_FILE_ATTR_READONLY
                       | APR_FILE_ATTR_HIDDEN)))
        return APR_SUCCESS;

    rc = DosQueryPathInfo(fname, FIL_STANDARD, &fs3, sizeof(fs3));
    if (rc == 0) {
        ULONG old_attr = fs3.attrFile;

        if (attr_mask & APR_FILE_ATTR_READONLY)
        {
            if (attributes & APR_FILE_ATTR_READONLY) {
                fs3.attrFile |= FILE_READONLY;
            } else {
                fs3.attrFile &= ~FILE_READONLY;
            }
        }

        if (attr_mask & APR_FILE_ATTR_HIDDEN)
        {
            if (attributes & APR_FILE_ATTR_HIDDEN) {
                fs3.attrFile |= FILE_HIDDEN;
            } else {
                fs3.attrFile &= ~FILE_HIDDEN;
            }
        }

        if (fs3.attrFile != old_attr) {
            rc = DosSetPathInfo(fname, FIL_STANDARD, &fs3, sizeof(fs3), 0);
        }
    }

    return APR_FROM_OS_ERROR(rc);
}


/* ### Somebody please write this! */
APR_DECLARE(fspr_status_t) fspr_file_mtime_set(const char *fname,
                                              fspr_time_t mtime,
                                              fspr_pool_t *pool)
{
    FILESTATUS3 fs3;
    ULONG rc;
    rc = DosQueryPathInfo(fname, FIL_STANDARD, &fs3, sizeof(fs3));

    if (rc) {
        return APR_FROM_OS_ERROR(rc);
    }

    fspr_fspr_time_to_os2_time(&fs3.fdateLastWrite, &fs3.ftimeLastWrite, mtime);

    rc = DosSetPathInfo(fname, FIL_STANDARD, &fs3, sizeof(fs3), 0);
    return APR_FROM_OS_ERROR(rc);
}
