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
#include "fspr_strings.h"
#include "fspr_portable.h"
#include "fspr_thread_mutex.h"
#include "fspr_arch_inherit.h"

#ifdef NETWARE
#include "nks/dirio.h"
#include "fspr_hash.h"
#include "fsio.h"
#endif

fspr_status_t fspr_unix_file_cleanup(void *thefile)
{
    fspr_file_t *file = thefile;
    fspr_status_t flush_rv = APR_SUCCESS, rv = APR_SUCCESS;

    if (file->buffered) {
        flush_rv = fspr_file_flush(file);
    }
    if (close(file->filedes) == 0) {
        file->filedes = -1;
        if (file->flags & APR_DELONCLOSE) {
            unlink(file->fname);
        }
#if APR_HAS_THREADS
        if (file->thlock) {
            rv = fspr_thread_mutex_destroy(file->thlock);
        }
#endif
    }
    else {
        /* Are there any error conditions other than EINTR or EBADF? */
        rv = errno;
    }
#ifndef WAITIO_USES_POLL
    if (file->pollset != NULL) {
        int pollset_rv = fspr_pollset_destroy(file->pollset);
        /* If the file close failed, return its error value,
         * not fspr_pollset_destroy()'s.
         */
        if (rv == APR_SUCCESS) {
            rv = pollset_rv;
        }
    }
#endif /* !WAITIO_USES_POLL */
    return rv != APR_SUCCESS ? rv : flush_rv;
}

APR_DECLARE(fspr_status_t) fspr_file_open(fspr_file_t **new, 
                                        const char *fname, 
                                        fspr_int32_t flag, 
                                        fspr_fileperms_t perm, 
                                        fspr_pool_t *pool)
{
    fspr_os_file_t fd;
    int oflags = 0;
#if APR_HAS_THREADS
    fspr_thread_mutex_t *thlock;
    fspr_status_t rv;
#endif

    if ((flag & APR_READ) && (flag & APR_WRITE)) {
        oflags = O_RDWR;
    }
    else if (flag & APR_READ) {
        oflags = O_RDONLY;
    }
    else if (flag & APR_WRITE) {
        oflags = O_WRONLY;
    }
    else {
        return APR_EACCES; 
    }

    if (flag & APR_CREATE) {
        oflags |= O_CREAT; 
        if (flag & APR_EXCL) {
            oflags |= O_EXCL;
        }
    }
    if ((flag & APR_EXCL) && !(flag & APR_CREATE)) {
        return APR_EACCES;
    }   

    if (flag & APR_APPEND) {
        oflags |= O_APPEND;
    }
    if (flag & APR_TRUNCATE) {
        oflags |= O_TRUNC;
    }
#ifdef O_BINARY
    if (flag & APR_BINARY) {
        oflags |= O_BINARY;
    }
#endif
    
#if APR_HAS_LARGE_FILES && defined(_LARGEFILE64_SOURCE)
    oflags |= O_LARGEFILE;
#elif defined(O_LARGEFILE)
    if (flag & APR_LARGEFILE) {
        oflags |= O_LARGEFILE;
    }
#endif

#if APR_HAS_THREADS
    if ((flag & APR_BUFFERED) && (flag & APR_XTHREAD)) {
        rv = fspr_thread_mutex_create(&thlock,
                                     APR_THREAD_MUTEX_DEFAULT, pool);
        if (rv) {
            return rv;
        }
    }
#endif

    if (perm == APR_OS_DEFAULT) {
        fd = open(fname, oflags, 0666);
    }
    else {
        fd = open(fname, oflags, fspr_unix_perms2mode(perm));
    } 
    if (fd < 0) {
       return errno;
    }

    (*new) = (fspr_file_t *)fspr_pcalloc(pool, sizeof(fspr_file_t));
    (*new)->pool = pool;
    (*new)->flags = flag;
    (*new)->filedes = fd;

    (*new)->fname = fspr_pstrdup(pool, fname);

    (*new)->blocking = BLK_ON;
    (*new)->buffered = (flag & APR_BUFFERED) > 0;

    if ((*new)->buffered) {
        (*new)->buffer = fspr_palloc(pool, APR_FILE_BUFSIZE);
#if APR_HAS_THREADS
        if ((*new)->flags & APR_XTHREAD) {
            (*new)->thlock = thlock;
        }
#endif
    }
    else {
        (*new)->buffer = NULL;
    }

    (*new)->is_pipe = 0;
    (*new)->timeout = -1;
    (*new)->ungetchar = -1;
    (*new)->eof_hit = 0;
    (*new)->filePtr = 0;
    (*new)->bufpos = 0;
    (*new)->dataRead = 0;
    (*new)->direction = 0;
#ifndef WAITIO_USES_POLL
    /* Start out with no pollset.  fspr_wait_for_io_or_timeout() will
     * initialize the pollset if needed.
     */
    (*new)->pollset = NULL;
#endif
    if (!(flag & APR_FILE_NOCLEANUP)) {
        fspr_pool_cleanup_register((*new)->pool, (void *)(*new), 
                                  fspr_unix_file_cleanup, 
                                  fspr_unix_file_cleanup);
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_file_close(fspr_file_t *file)
{
    return fspr_pool_cleanup_run(file->pool, file, fspr_unix_file_cleanup);
}

APR_DECLARE(fspr_status_t) fspr_file_remove(const char *path, fspr_pool_t *pool)
{
    if (unlink(path) == 0) {
        return APR_SUCCESS;
    }
    else {
        return errno;
    }
}

APR_DECLARE(fspr_status_t) fspr_file_rename(const char *from_path, 
                                          const char *to_path,
                                          fspr_pool_t *p)
{
    if (rename(from_path, to_path) != 0) {
        return errno;
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_file_get(fspr_os_file_t *thefile, 
                                          fspr_file_t *file)
{
    *thefile = file->filedes;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_file_put(fspr_file_t **file, 
                                          fspr_os_file_t *thefile,
                                          fspr_int32_t flags, fspr_pool_t *pool)
{
    int *dafile = thefile;
    
    (*file) = fspr_pcalloc(pool, sizeof(fspr_file_t));
    (*file)->pool = pool;
    (*file)->eof_hit = 0;
    (*file)->blocking = BLK_UNKNOWN; /* in case it is a pipe */
    (*file)->timeout = -1;
    (*file)->ungetchar = -1; /* no char avail */
    (*file)->filedes = *dafile;
    (*file)->flags = flags | APR_FILE_NOCLEANUP;
    (*file)->buffered = (flags & APR_BUFFERED) > 0;

#ifndef WAITIO_USES_POLL
    /* Start out with no pollset.  fspr_wait_for_io_or_timeout() will
     * initialize the pollset if needed.
     */
    (*file)->pollset = NULL;
#endif

    if ((*file)->buffered) {
        (*file)->buffer = fspr_palloc(pool, APR_FILE_BUFSIZE);
#if APR_HAS_THREADS
        if ((*file)->flags & APR_XTHREAD) {
            fspr_status_t rv;
            rv = fspr_thread_mutex_create(&((*file)->thlock),
                                         APR_THREAD_MUTEX_DEFAULT, pool);
            if (rv) {
                return rv;
            }
        }
#endif
    }
    return APR_SUCCESS;
}    

APR_DECLARE(fspr_status_t) fspr_file_eof(fspr_file_t *fptr)
{
    if (fptr->eof_hit == 1) {
        return APR_EOF;
    }
    return APR_SUCCESS;
}   

APR_DECLARE(fspr_status_t) fspr_file_open_stderr(fspr_file_t **thefile, 
                                               fspr_pool_t *pool)
{
    int fd = STDERR_FILENO;

    return fspr_os_file_put(thefile, &fd, 0, pool);
}

APR_DECLARE(fspr_status_t) fspr_file_open_stdout(fspr_file_t **thefile, 
                                               fspr_pool_t *pool)
{
    int fd = STDOUT_FILENO;

    return fspr_os_file_put(thefile, &fd, 0, pool);
}

APR_DECLARE(fspr_status_t) fspr_file_open_stdin(fspr_file_t **thefile, 
                                              fspr_pool_t *pool)
{
    int fd = STDIN_FILENO;

    return fspr_os_file_put(thefile, &fd, 0, pool);
}

APR_IMPLEMENT_INHERIT_SET(file, flags, pool, fspr_unix_file_cleanup)

APR_IMPLEMENT_INHERIT_UNSET(file, flags, pool, fspr_unix_file_cleanup)

APR_POOL_IMPLEMENT_ACCESSOR(file)
