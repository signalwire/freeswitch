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
#include "fspr_general.h"
#include "fspr_strings.h"
#include "fspr_lib.h"
#include "fspr_errno.h"
#include <malloc.h>
#include "fspr_arch_atime.h"
#include "fspr_arch_misc.h"

/*
 * read_with_timeout() 
 * Uses async i/o to emulate unix non-blocking i/o with timeouts.
 */
static fspr_status_t read_with_timeout(fspr_file_t *file, void *buf, fspr_size_t len_in, fspr_size_t *nbytes)
{
    fspr_status_t rv;
    DWORD len = (DWORD)len_in;
    DWORD bytesread = 0;

    /* Handle the zero timeout non-blocking case */
    if (file->timeout == 0) {
        /* Peek at the pipe. If there is no data available, return APR_EAGAIN.
         * If data is available, go ahead and read it.
         */
        if (file->pipe) {
            DWORD bytes;
            if (!PeekNamedPipe(file->filehand, NULL, 0, NULL, &bytes, NULL)) {
                rv = fspr_get_os_error();
                if (rv == APR_FROM_OS_ERROR(ERROR_BROKEN_PIPE)) {
                    rv = APR_EOF;
                }
                *nbytes = 0;
                return rv;
            }
            else {
                if (bytes == 0) {
                    *nbytes = 0;
                    return APR_EAGAIN;
                }
                if (len > bytes) {
                    len = bytes;
                }
            }
        }
        else {
            /* ToDo: Handle zero timeout non-blocking file i/o 
             * This is not needed until an APR application needs to
             * timeout file i/o (which means setting file i/o non-blocking)
             */
        }
    }

    if (file->pOverlapped && !file->pipe) {
        file->pOverlapped->Offset     = (DWORD)file->filePtr;
        file->pOverlapped->OffsetHigh = (DWORD)(file->filePtr >> 32);
    }

    rv = ReadFile(file->filehand, buf, len, 
                  &bytesread, file->pOverlapped);
    *nbytes = bytesread;

    if (!rv) {
        rv = fspr_get_os_error();
        if (rv == APR_FROM_OS_ERROR(ERROR_IO_PENDING)) {
            /* Wait for the pending i/o */
            if (file->timeout > 0) {
                /* timeout in milliseconds... */
                rv = WaitForSingleObject(file->pOverlapped->hEvent, 
                                         (DWORD)(file->timeout/1000)); 
            }
            else if (file->timeout == -1) {
                rv = WaitForSingleObject(file->pOverlapped->hEvent, INFINITE);
            }
            switch (rv) {
                case WAIT_OBJECT_0:
                    GetOverlappedResult(file->filehand, file->pOverlapped, 
                                        &bytesread, TRUE);
                    *nbytes = bytesread;
                    rv = APR_SUCCESS;
                    break;

                case WAIT_TIMEOUT:
                    rv = APR_TIMEUP;
                    break;

                case WAIT_FAILED:
                    rv = fspr_get_os_error();
                    break;

                default:
                    break;
            }

            if (rv != APR_SUCCESS) {
                if (fspr_os_level >= APR_WIN_98) {
                    CancelIo(file->filehand);
                }
            }
        }
        else if (rv == APR_FROM_OS_ERROR(ERROR_BROKEN_PIPE)) {
            /* Assume ERROR_BROKEN_PIPE signals an EOF reading from a pipe */
            rv = APR_EOF;
        }
    } else {
        /* OK and 0 bytes read ==> end of file */
        if (*nbytes == 0)
            rv = APR_EOF;
        else
            rv = APR_SUCCESS;
    }
    if (rv == APR_SUCCESS && file->pOverlapped && !file->pipe) {
        file->filePtr += *nbytes;
    }
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_file_read(fspr_file_t *thefile, void *buf, fspr_size_t *len)
{
    fspr_status_t rv;
    DWORD bytes_read = 0;

    if (*len <= 0) {
        *len = 0;
        return APR_SUCCESS;
    }

    /* If the file is open for xthread support, allocate and
     * initialize the overlapped and io completion event (hEvent). 
     * Threads should NOT share an fspr_file_t or its hEvent.
     */
    if ((thefile->flags & APR_XTHREAD) && !thefile->pOverlapped ) {
        thefile->pOverlapped = (OVERLAPPED*) fspr_pcalloc(thefile->pool, 
                                                         sizeof(OVERLAPPED));
        thefile->pOverlapped->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!thefile->pOverlapped->hEvent) {
            rv = fspr_get_os_error();
            return rv;
        }
    }

    /* Handle the ungetchar if there is one */
    if (thefile->ungetchar != -1) {
        bytes_read = 1;
        *(char *)buf = (char)thefile->ungetchar;
        buf = (char *)buf + 1;
        (*len)--;
        thefile->ungetchar = -1;
        if (*len == 0) {
            *len = bytes_read;
            return APR_SUCCESS;
        }
    }
    if (thefile->buffered) {
        char *pos = (char *)buf;
        fspr_size_t blocksize;
        fspr_size_t size = *len;

        fspr_thread_mutex_lock(thefile->mutex);

        if (thefile->direction == 1) {
            rv = fspr_file_flush(thefile);
            if (rv != APR_SUCCESS) {
                fspr_thread_mutex_unlock(thefile->mutex);
                return rv;
            }
            thefile->bufpos = 0;
            thefile->direction = 0;
            thefile->dataRead = 0;
        }

        rv = 0;
        while (rv == 0 && size > 0) {
            if (thefile->bufpos >= thefile->dataRead) {
                fspr_size_t read;
                rv = read_with_timeout(thefile, thefile->buffer, 
                                       APR_FILE_BUFSIZE, &read);
                if (read == 0) {
                    if (rv == APR_EOF)
                        thefile->eof_hit = TRUE;
                    break;
                }
                else {
                    thefile->dataRead = read;
                    thefile->filePtr += thefile->dataRead;
                    thefile->bufpos = 0;
                }
            }

            blocksize = size > thefile->dataRead - thefile->bufpos ? thefile->dataRead - thefile->bufpos : size;
            memcpy(pos, thefile->buffer + thefile->bufpos, blocksize);
            thefile->bufpos += blocksize;
            pos += blocksize;
            size -= blocksize;
        }

        *len = pos - (char *)buf;
        if (*len) {
            rv = APR_SUCCESS;
        }
        fspr_thread_mutex_unlock(thefile->mutex);
    } else {  
        /* Unbuffered i/o */
        fspr_size_t nbytes;
        rv = read_with_timeout(thefile, buf, *len, &nbytes);
        if (rv == APR_EOF)
            thefile->eof_hit = TRUE;
        *len = nbytes;
    }

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_file_write(fspr_file_t *thefile, const void *buf, fspr_size_t *nbytes)
{
    fspr_status_t rv;
    DWORD bwrote;

    /* If the file is open for xthread support, allocate and
     * initialize the overlapped and io completion event (hEvent). 
     * Threads should NOT share an fspr_file_t or its hEvent.
     */
    if ((thefile->flags & APR_XTHREAD) && !thefile->pOverlapped ) {
        thefile->pOverlapped = (OVERLAPPED*) fspr_pcalloc(thefile->pool, 
                                                         sizeof(OVERLAPPED));
        thefile->pOverlapped->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!thefile->pOverlapped->hEvent) {
            rv = fspr_get_os_error();
            return rv;
        }
    }

    if (thefile->buffered) {
        char *pos = (char *)buf;
        fspr_size_t blocksize;
        fspr_size_t size = *nbytes;

        fspr_thread_mutex_lock(thefile->mutex);

        if (thefile->direction == 0) {
            // Position file pointer for writing at the offset we are logically reading from
            fspr_off_t offset = thefile->filePtr - thefile->dataRead + thefile->bufpos;
            DWORD offlo = (DWORD)offset;
            DWORD offhi = (DWORD)(offset >> 32);
            if (offset != thefile->filePtr)
                SetFilePointer(thefile->filehand, offlo, &offhi, FILE_BEGIN);
            thefile->bufpos = thefile->dataRead = 0;
            thefile->direction = 1;
        }

        rv = 0;
        while (rv == 0 && size > 0) {
            if (thefile->bufpos == APR_FILE_BUFSIZE)   // write buffer is full
                rv = fspr_file_flush(thefile);

            blocksize = size > APR_FILE_BUFSIZE - thefile->bufpos ? APR_FILE_BUFSIZE - thefile->bufpos : size;
            memcpy(thefile->buffer + thefile->bufpos, pos, blocksize);
            thefile->bufpos += blocksize;
            pos += blocksize;
            size -= blocksize;
        }

        fspr_thread_mutex_unlock(thefile->mutex);
        return rv;
    } else {
        if (!thefile->pipe) {
            fspr_off_t offset = 0;
            fspr_status_t rc;
            if (thefile->append) {
                /* fspr_file_lock will mutex the file across processes.
                 * The call to fspr_thread_mutex_lock is added to avoid
                 * a race condition between LockFile and WriteFile 
                 * that occasionally leads to deadlocked threads.
                 */
                fspr_thread_mutex_lock(thefile->mutex);
                rc = fspr_file_lock(thefile, APR_FLOCK_EXCLUSIVE);
                if (rc != APR_SUCCESS) {
                    fspr_thread_mutex_unlock(thefile->mutex);
                    return rc;
                }
                rc = fspr_file_seek(thefile, APR_END, &offset);
                if (rc != APR_SUCCESS) {
                    fspr_thread_mutex_unlock(thefile->mutex);
                    return rc;
                }
            }
            if (thefile->pOverlapped) {
                thefile->pOverlapped->Offset     = (DWORD)thefile->filePtr;
                thefile->pOverlapped->OffsetHigh = (DWORD)(thefile->filePtr >> 32);
            }
            rv = WriteFile(thefile->filehand, buf, (DWORD)*nbytes, &bwrote,
                           thefile->pOverlapped);
            if (thefile->append) {
                fspr_file_unlock(thefile);
                fspr_thread_mutex_unlock(thefile->mutex);
            }
        }
        else {
            rv = WriteFile(thefile->filehand, buf, (DWORD)*nbytes, &bwrote,
                           thefile->pOverlapped);
        }
        if (rv) {
            *nbytes = bwrote;
            rv = APR_SUCCESS;
        }
        else {
            (*nbytes) = 0;
            rv = fspr_get_os_error();
            if (rv == APR_FROM_OS_ERROR(ERROR_IO_PENDING)) {
 
                DWORD timeout_ms;

                if (thefile->timeout == 0) {
                    timeout_ms = 0;
                }
                else if (thefile->timeout < 0) {
                    timeout_ms = INFINITE;
                }
                else {
                    timeout_ms = (DWORD)(thefile->timeout / 1000);
                }
	       
                rv = WaitForSingleObject(thefile->pOverlapped->hEvent, timeout_ms);
                switch (rv) {
                    case WAIT_OBJECT_0:
                        GetOverlappedResult(thefile->filehand, thefile->pOverlapped, 
                                            &bwrote, TRUE);
                        *nbytes = bwrote;
                        rv = APR_SUCCESS;
                        break;
                    case WAIT_TIMEOUT:
                        rv = APR_TIMEUP;
                        break;
                    case WAIT_FAILED:
                        rv = fspr_get_os_error();
                        break;
                    default:
                        break;
                }
                if (rv != APR_SUCCESS) {
                    if (fspr_os_level >= APR_WIN_98)
                        CancelIo(thefile->filehand);
                }
            }
        }
        if (rv == APR_SUCCESS && thefile->pOverlapped && !thefile->pipe) {
            thefile->filePtr += *nbytes;
        }
    }
    return rv;
}
/* ToDo: Write for it anyway and test the oslevel!
 * Too bad WriteFileGather() is not supported on 95&98 (or NT prior to SP2)
 */
APR_DECLARE(fspr_status_t) fspr_file_writev(fspr_file_t *thefile,
                                     const struct iovec *vec,
                                     fspr_size_t nvec, 
                                     fspr_size_t *nbytes)
{
    fspr_status_t rv = APR_SUCCESS;
    fspr_size_t i;
    fspr_size_t bwrote = 0;
    char *buf;

    *nbytes = 0;
    for (i = 0; i < nvec; i++) {
        buf = vec[i].iov_base;
        bwrote = vec[i].iov_len;
        rv = fspr_file_write(thefile, buf, &bwrote);
        *nbytes += bwrote;
        if (rv != APR_SUCCESS) {
            break;
        }
    }
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_file_putc(char ch, fspr_file_t *thefile)
{
    fspr_size_t len = 1;

    return fspr_file_write(thefile, &ch, &len);
}

APR_DECLARE(fspr_status_t) fspr_file_ungetc(char ch, fspr_file_t *thefile)
{
    thefile->ungetchar = (unsigned char) ch;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_file_getc(char *ch, fspr_file_t *thefile)
{
    fspr_status_t rc;
    fspr_size_t bread;

    bread = 1;
    rc = fspr_file_read(thefile, ch, &bread);

    if (rc) {
        return rc;
    }
    
    if (bread == 0) {
        thefile->eof_hit = TRUE;
        return APR_EOF;
    }
    return APR_SUCCESS; 
}

APR_DECLARE(fspr_status_t) fspr_file_puts(const char *str, fspr_file_t *thefile)
{
    fspr_size_t len = strlen(str);

    return fspr_file_write(thefile, str, &len);
}

APR_DECLARE(fspr_status_t) fspr_file_gets(char *str, int len, fspr_file_t *thefile)
{
    fspr_size_t readlen;
    fspr_status_t rv = APR_SUCCESS;
    int i;    

    for (i = 0; i < len-1; i++) {
        readlen = 1;
        rv = fspr_file_read(thefile, str+i, &readlen);

        if (rv != APR_SUCCESS && rv != APR_EOF)
            return rv;

        if (readlen == 0) {
            /* If we have bytes, defer APR_EOF to the next call */
            if (i > 0)
                rv = APR_SUCCESS;
            break;
        }
        
        if (str[i] == '\n') {
            i++; /* don't clobber this char below */
            break;
        }
    }
    str[i] = 0;
    return rv;
}

APR_DECLARE(fspr_status_t) fspr_file_flush(fspr_file_t *thefile)
{
    if (thefile->buffered) {
        DWORD numbytes, written = 0;
        fspr_status_t rc = 0;
        char *buffer;
        fspr_size_t bytesleft;

        if (thefile->direction == 1 && thefile->bufpos) {
            buffer = thefile->buffer;
            bytesleft = thefile->bufpos;           

            do {
                if (bytesleft > APR_DWORD_MAX) {
                    numbytes = APR_DWORD_MAX;
                }
                else {
                    numbytes = (DWORD)bytesleft;
                }

                if (!WriteFile(thefile->filehand, buffer, numbytes, &written, NULL)) {
                    rc = fspr_get_os_error();
                    thefile->filePtr += written;
                    break;
                }

                thefile->filePtr += written;
                bytesleft -= written;
                buffer += written;

            } while (bytesleft > 0);

            if (rc == 0)
                thefile->bufpos = 0;
        }

        return rc;
    }

    /* There isn't anything to do if we aren't buffering the output
     * so just return success.
     */
    return APR_SUCCESS; 
}

struct fspr_file_printf_data {
    fspr_vformatter_buff_t vbuff;
    fspr_file_t *fptr;
    char *buf;
};

static int file_printf_flush(fspr_vformatter_buff_t *buff)
{
    struct fspr_file_printf_data *data = (struct fspr_file_printf_data *)buff;

    if (fspr_file_write_full(data->fptr, data->buf,
                            data->vbuff.curpos - data->buf, NULL)) {
        return -1;
    }

    data->vbuff.curpos = data->buf;
    return 0;
}

APR_DECLARE_NONSTD(int) fspr_file_printf(fspr_file_t *fptr, 
                                        const char *format, ...)
{
    struct fspr_file_printf_data data;
    va_list ap;
    int count;

    data.buf = malloc(HUGE_STRING_LEN);
    if (data.buf == NULL) {
        return 0;
    }
    data.vbuff.curpos = data.buf;
    data.vbuff.endpos = data.buf + HUGE_STRING_LEN;
    data.fptr = fptr;
    va_start(ap, format);
    count = fspr_vformatter(file_printf_flush,
                           (fspr_vformatter_buff_t *)&data, format, ap);
    /* fspr_vformatter does not call flush for the last bits */
    if (count >= 0) file_printf_flush((fspr_vformatter_buff_t *)&data);

    va_end(ap);

    free(data.buf);
    return count;
}
