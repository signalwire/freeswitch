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

#include "apr_arch_file_io.h"
#include "apr_strings.h"
#include "apr_thread_mutex.h"
#include "apr_support.h"

/* The only case where we don't use wait_for_io_or_timeout is on
 * pre-BONE BeOS, so this check should be sufficient and simpler */
#if !BEOS_R5
#define USE_WAIT_FOR_IO
#endif

APR_DECLARE(apr_status_t) apr_file_read(apr_file_t *thefile, void *buf, apr_size_t *nbytes)
{
    apr_ssize_t rv;
    apr_size_t bytes_read;

    if (*nbytes <= 0) {
        *nbytes = 0;
        return APR_SUCCESS;
    }

    if (thefile->buffered) {
        char *pos = (char *)buf;
        apr_uint64_t blocksize;
        apr_uint64_t size = *nbytes;

#if APR_HAS_THREADS
        if (thefile->thlock) {
            apr_thread_mutex_lock(thefile->thlock);
        }
#endif

        if (thefile->direction == 1) {
            rv = apr_file_flush(thefile);
            if (rv) {
#if APR_HAS_THREADS
                if (thefile->thlock) {
                    apr_thread_mutex_unlock(thefile->thlock);
                }
#endif
                return rv;
            }
            thefile->bufpos = 0;
            thefile->direction = 0;
            thefile->dataRead = 0;
        }

        rv = 0;
        if (thefile->ungetchar != -1) {
            *pos = (char)thefile->ungetchar;
            ++pos;
            --size;
            thefile->ungetchar = -1;
        }
        while (rv == 0 && size > 0) {
            if (thefile->bufpos >= thefile->dataRead) {
                int bytesread = read(thefile->filedes, thefile->buffer, APR_FILE_BUFSIZE);
                if (bytesread == 0) {
                    thefile->eof_hit = TRUE;
                    rv = APR_EOF;
                    break;
                }
                else if (bytesread == -1) {
                    rv = errno;
                    break;
                }
                thefile->dataRead = bytesread;
                thefile->filePtr += thefile->dataRead;
                thefile->bufpos = 0;
            }

            blocksize = size > thefile->dataRead - thefile->bufpos ? thefile->dataRead - thefile->bufpos : size;
            memcpy(pos, thefile->buffer + thefile->bufpos, blocksize);
            thefile->bufpos += blocksize;
            pos += blocksize;
            size -= blocksize;
        }

        *nbytes = pos - (char *)buf;
        if (*nbytes) {
            rv = 0;
        }
#if APR_HAS_THREADS
        if (thefile->thlock) {
            apr_thread_mutex_unlock(thefile->thlock);
        }
#endif
        return rv;
    }
    else {
        bytes_read = 0;
        if (thefile->ungetchar != -1) {
            bytes_read = 1;
            *(char *)buf = (char)thefile->ungetchar;
            buf = (char *)buf + 1;
            (*nbytes)--;
            thefile->ungetchar = -1;
            if (*nbytes == 0) {
                *nbytes = bytes_read;
                return APR_SUCCESS;
            }
        }

        do {
            rv = read(thefile->filedes, buf, *nbytes);
        } while (rv == -1 && errno == EINTR);
#ifdef USE_WAIT_FOR_IO
        if (rv == -1 && 
            (errno == EAGAIN || errno == EWOULDBLOCK) && 
            thefile->timeout != 0) {
            apr_status_t arv = apr_wait_for_io_or_timeout(thefile, NULL, 1);
            if (arv != APR_SUCCESS) {
                *nbytes = bytes_read;
                return arv;
            }
            else {
                do {
                    rv = read(thefile->filedes, buf, *nbytes);
                } while (rv == -1 && errno == EINTR);
            }
        }  
#endif
        *nbytes = bytes_read;
        if (rv == 0) {
            thefile->eof_hit = TRUE;
            return APR_EOF;
        }
        if (rv > 0) {
            *nbytes += rv;
            return APR_SUCCESS;
        }
        return errno;
    }
}

APR_DECLARE(apr_status_t) apr_file_write(apr_file_t *thefile, const void *buf, apr_size_t *nbytes)
{
    apr_size_t rv;

    if (thefile->buffered) {
        char *pos = (char *)buf;
        int blocksize;
        int size = *nbytes;

#if APR_HAS_THREADS
        if (thefile->thlock) {
            apr_thread_mutex_lock(thefile->thlock);
        }
#endif

        if ( thefile->direction == 0 ) {
            /* Position file pointer for writing at the offset we are 
             * logically reading from
             */
            apr_int64_t offset = thefile->filePtr - thefile->dataRead + thefile->bufpos;
            if (offset != thefile->filePtr)
                lseek(thefile->filedes, offset, SEEK_SET);
            thefile->bufpos = thefile->dataRead = 0;
            thefile->direction = 1;
        }

        rv = 0;
        while (rv == 0 && size > 0) {
            if (thefile->bufpos == APR_FILE_BUFSIZE)   /* write buffer is full*/
                rv = apr_file_flush(thefile);

            blocksize = size > APR_FILE_BUFSIZE - thefile->bufpos ? 
                        APR_FILE_BUFSIZE - thefile->bufpos : size;
            memcpy(thefile->buffer + thefile->bufpos, pos, blocksize);                      
            thefile->bufpos += blocksize;
            pos += blocksize;
            size -= blocksize;
        }

#if APR_HAS_THREADS
        if (thefile->thlock) {
            apr_thread_mutex_unlock(thefile->thlock);
        }
#endif
        return rv;
    }
    else {
        do {
            rv = write(thefile->filedes, buf, *nbytes);
        } while (rv == (apr_size_t)-1 && errno == EINTR);
#ifdef USE_WAIT_FOR_IO
        if (rv == (apr_size_t)-1 &&
            (errno == EAGAIN || errno == EWOULDBLOCK) && 
            thefile->timeout != 0) {
            apr_status_t arv = apr_wait_for_io_or_timeout(thefile, NULL, 0);
            if (arv != APR_SUCCESS) {
                *nbytes = 0;
                return arv;
            }
            else {
                do {
                    do {
                        rv = write(thefile->filedes, buf, *nbytes);
                    } while (rv == (apr_size_t)-1 && errno == EINTR);
                    if (rv == (apr_size_t)-1 &&
                        (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        *nbytes /= 2; /* yes, we'll loop if kernel lied
                                       * and we can't even write 1 byte
                                       */
                    }
                    else {
                        break;
                    }
                } while (1);
            }
        }  
#endif
        if (rv == (apr_size_t)-1) {
            (*nbytes) = 0;
            return errno;
        }
        *nbytes = rv;
        return APR_SUCCESS;
    }
}

APR_DECLARE(apr_status_t) apr_file_writev(apr_file_t *thefile, const struct iovec *vec,
                                          apr_size_t nvec, apr_size_t *nbytes)
{
#ifdef HAVE_WRITEV
    int bytes;

    if ((bytes = writev(thefile->filedes, vec, nvec)) < 0) {
        *nbytes = 0;
        return errno;
    }
    else {
        *nbytes = bytes;
        return APR_SUCCESS;
    }
#else
    /**
     * The problem with trying to output the entire iovec is that we cannot
     * maintain the behavoir that a real writev would have.  If we iterate
     * over the iovec one at a time, we loose the atomic properties of 
     * writev().  The other option is to combine the entire iovec into one
     * buffer that we could then send in one call to write().  This is not 
     * reasonable since we do not know how much data an iovec could contain.
     *
     * The only reasonable option, that maintains the semantics of a real 
     * writev(), is to only write the first iovec.  Callers of file_writev()
     * must deal with partial writes as they normally would. If you want to 
     * ensure an entire iovec is written, use apr_file_writev_full().
     */

    *nbytes = vec[0].iov_len;
    return apr_file_write(thefile, vec[0].iov_base, nbytes);
#endif
}

APR_DECLARE(apr_status_t) apr_file_putc(char ch, apr_file_t *thefile)
{
    apr_size_t nbytes = 1;

    return apr_file_write(thefile, &ch, &nbytes);
}

APR_DECLARE(apr_status_t) apr_file_ungetc(char ch, apr_file_t *thefile)
{
    thefile->ungetchar = (unsigned char)ch;
    return APR_SUCCESS; 
}

APR_DECLARE(apr_status_t) apr_file_getc(char *ch, apr_file_t *thefile)
{
    apr_size_t nbytes = 1;

    return apr_file_read(thefile, ch, &nbytes);
}

APR_DECLARE(apr_status_t) apr_file_puts(const char *str, apr_file_t *thefile)
{
    return apr_file_write_full(thefile, str, strlen(str), NULL);
}

APR_DECLARE(apr_status_t) apr_file_flush(apr_file_t *thefile)
{
    if (thefile->buffered) {
        apr_int64_t written = 0;

        if (thefile->direction == 1 && thefile->bufpos) {
            do {
                written = write(thefile->filedes, thefile->buffer, thefile->bufpos);
            } while (written == (apr_int64_t)-1 && errno == EINTR);
            if (written == (apr_int64_t)-1) {
                return errno;
            }
            thefile->filePtr += written;
            thefile->bufpos = 0;
        }
    }
    /* There isn't anything to do if we aren't buffering the output
     * so just return success.
     */
    return APR_SUCCESS; 
}

APR_DECLARE(apr_status_t) apr_file_gets(char *str, int len, apr_file_t *thefile)
{
    apr_status_t rv = APR_SUCCESS; /* get rid of gcc warning */
    apr_size_t nbytes;
    const char *str_start = str;
    char *final = str + len - 1;

    if (len <= 1) {  
        /* sort of like fgets(), which returns NULL and stores no bytes 
         */
        return APR_SUCCESS;
    }

    /* If we have an underlying buffer, we can be *much* more efficient
     * and skip over the apr_file_read calls.
     */
    if (thefile->buffered) {

#if APR_HAS_THREADS
        if (thefile->thlock) {
            apr_thread_mutex_lock(thefile->thlock);
        }
#endif

        if (thefile->direction == 1) {
            rv = apr_file_flush(thefile);
            if (rv) {
#if APR_HAS_THREADS
                if (thefile->thlock) {
                    apr_thread_mutex_unlock(thefile->thlock);
                }
#endif
                return rv;
            }

            thefile->direction = 0;
            thefile->bufpos = 0;
            thefile->dataRead = 0;
        }

        while (str < final) { /* leave room for trailing '\0' */
            /* Force ungetc leftover to call apr_file_read. */
            if (thefile->bufpos < thefile->dataRead &&
                thefile->ungetchar == -1) {
                *str = thefile->buffer[thefile->bufpos++];
            }
            else {
                nbytes = 1;
                rv = apr_file_read(thefile, str, &nbytes);
                if (rv != APR_SUCCESS) {
                    break;
                }
            }
            if (*str == '\n') {
                ++str;
                break;
            }
            ++str;
        }

#if APR_HAS_THREADS
        if (thefile->thlock) {
            apr_thread_mutex_unlock(thefile->thlock);
        }
#endif
    }
    else {
        while (str < final) { /* leave room for trailing '\0' */
            nbytes = 1;
            rv = apr_file_read(thefile, str, &nbytes);
            if (rv != APR_SUCCESS) {
                break;
            }
            if (*str == '\n') {
                ++str;
                break;
            }
            ++str;
        }
    }

    /* We must store a terminating '\0' if we've stored any chars. We can
     * get away with storing it if we hit an error first. 
     */
    *str = '\0';
    if (str > str_start) {
        /* we stored chars; don't report EOF or any other errors;
         * the app will find out about that on the next call
         */
        return APR_SUCCESS;
    }
    return rv;
}

struct apr_file_printf_data {
    apr_vformatter_buff_t vbuff;
    apr_file_t *fptr;
    char *buf;
};

static int file_printf_flush(apr_vformatter_buff_t *buff)
{
    struct apr_file_printf_data *data = (struct apr_file_printf_data *)buff;

    if (apr_file_write_full(data->fptr, data->buf, 
                            data->vbuff.curpos - data->buf, NULL)) {
        return -1;
    }

    data->vbuff.curpos = data->buf;
    return 0;
}

APR_DECLARE_NONSTD(int) apr_file_printf(apr_file_t *fptr, 
                                        const char *format, ...)
{
    struct apr_file_printf_data data;
    va_list ap;
    int count;

    /* don't really need a HUGE_STRING_LEN anymore */
    data.buf = malloc(HUGE_STRING_LEN);
    if (data.buf == NULL) {
        return -1;
    }
    data.vbuff.curpos = data.buf;
    data.vbuff.endpos = data.buf + HUGE_STRING_LEN;
    data.fptr = fptr;
    va_start(ap, format);
    count = apr_vformatter(file_printf_flush,
                           (apr_vformatter_buff_t *)&data, format, ap);
    /* apr_vformatter does not call flush for the last bits */
    if (count >= 0) file_printf_flush((apr_vformatter_buff_t *)&data);

    va_end(ap);

    free(data.buf);

    return count;
}
