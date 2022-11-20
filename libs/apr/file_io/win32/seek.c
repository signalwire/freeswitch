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

#include "win32/fspr_arch_file_io.h"
#include "fspr_file_io.h"
#include <errno.h>
#include <string.h>

static fspr_status_t setptr(fspr_file_t *thefile, fspr_off_t pos )
{
    fspr_size_t newbufpos;
    fspr_status_t rv;
    DWORD rc;

    if (thefile->direction == 1) {
        /* XXX: flush here is not mutex protected */
        rv = fspr_file_flush(thefile);
        if (rv != APR_SUCCESS)
            return rv;
        thefile->bufpos = thefile->dataRead = 0;
        thefile->direction = 0;
    }

    /* We may be truncating to size here. 
     * XXX: testing an 'unsigned' as >= 0 below indicates a bug
     */
    newbufpos = (fspr_size_t)(pos - (thefile->filePtr 
                                  - thefile->dataRead));

    if (newbufpos >= 0 && newbufpos <= thefile->dataRead) {
        thefile->bufpos = (fspr_size_t)newbufpos;
        rv = APR_SUCCESS;
    } else {
        DWORD offlo = (DWORD)pos;
        DWORD offhi = (DWORD)(pos >> 32);
        rc = SetFilePointer(thefile->filehand, offlo, &offhi, FILE_BEGIN);

        if (rc == (DWORD)-1)
            /* A legal value, perhaps?  MSDN implies prior SetLastError isn't
             * needed, googling for SetLastError SetFilePointer seems
             * to confirm this.  INVALID_SET_FILE_POINTER is too recently
             * added for us to rely on it as a constant.
             */
            rv = fspr_get_os_error();
        else
            rv = APR_SUCCESS;

        if (rv == APR_SUCCESS) {
            rv = APR_SUCCESS;
            thefile->eof_hit = 0;
            thefile->bufpos = thefile->dataRead = 0;
            thefile->filePtr = pos;
        }
    }

    return rv;
}


APR_DECLARE(fspr_status_t) fspr_file_seek(fspr_file_t *thefile, fspr_seek_where_t where, fspr_off_t *offset)
{
    fspr_finfo_t finfo;
    fspr_status_t rc = APR_SUCCESS;

    thefile->eof_hit = 0;

    if (thefile->buffered) {
        switch (where) {
            case APR_SET:
                rc = setptr(thefile, *offset);
                break;

            case APR_CUR:
                rc = setptr(thefile, thefile->filePtr - thefile->dataRead 
                                      + thefile->bufpos + *offset);
                break;

            case APR_END:
                rc = fspr_file_info_get(&finfo, APR_FINFO_SIZE, thefile);
                if (rc == APR_SUCCESS)
                    rc = setptr(thefile, finfo.size + *offset);
                break;

            default:
                return APR_EINVAL;
        }

        *offset = thefile->filePtr - thefile->dataRead + thefile->bufpos;
        return rc;
    }
    /* A file opened with APR_XTHREAD has been opened for overlapped i/o. 
     * APR must explicitly track the file pointer in this case.
     */
    else if (thefile->pOverlapped || thefile->flags & APR_XTHREAD) {
        switch(where) {
            case APR_SET:
                thefile->filePtr = *offset;
                break;
        
            case APR_CUR:
                thefile->filePtr += *offset;
                break;
        
            case APR_END:
                rc = fspr_file_info_get(&finfo, APR_FINFO_SIZE, thefile);
                if (rc == APR_SUCCESS && finfo.size + *offset >= 0)
                    thefile->filePtr = finfo.size + *offset;
                break;

            default:
                return APR_EINVAL;
        }
        *offset = thefile->filePtr;
        return rc;
    }
    else {
        DWORD howmove;
        DWORD offlo = (DWORD)*offset;
        DWORD offhi = (DWORD)(*offset >> 32);

        switch(where) {
            case APR_SET:
                howmove = FILE_BEGIN;   break;
            case APR_CUR:
                howmove = FILE_CURRENT; break;
            case APR_END:
                howmove = FILE_END;     break;
            default:
                return APR_EINVAL;
        }
        offlo = SetFilePointer(thefile->filehand, (LONG)offlo, 
                               (LONG*)&offhi, howmove);
        if (offlo == 0xFFFFFFFF)
            rc = fspr_get_os_error();
        else
            rc = APR_SUCCESS;
        /* Since we can land at 0xffffffff we will measure our APR_SUCCESS */
        if (rc == APR_SUCCESS)
            *offset = ((fspr_off_t)offhi << 32) | offlo;
        return rc;
    }
}


APR_DECLARE(fspr_status_t) fspr_file_trunc(fspr_file_t *thefile, fspr_off_t offset)
{
    fspr_status_t rv;
    DWORD offlo = (DWORD)offset;
    DWORD offhi = (DWORD)(offset >> 32);
    DWORD rc;

    rc = SetFilePointer(thefile->filehand, offlo, &offhi, FILE_BEGIN);
    if (rc == 0xFFFFFFFF)
        if ((rv = fspr_get_os_error()) != APR_SUCCESS)
            return rv;

    if (!SetEndOfFile(thefile->filehand))
        return fspr_get_os_error();

    if (thefile->buffered) {
        return setptr(thefile, offset);
    }

    return APR_SUCCESS;
}
