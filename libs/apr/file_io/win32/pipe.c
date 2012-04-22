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

#include "win32/apr_arch_file_io.h"
#include "apr_file_io.h"
#include "apr_general.h"
#include "apr_strings.h"
#if APR_HAVE_ERRNO_H
#include <errno.h>
#endif
#include <string.h>
#include <stdio.h>
#if APR_HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#if APR_HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#include "apr_arch_misc.h"

APR_DECLARE(apr_status_t) apr_file_pipe_timeout_set(apr_file_t *thepipe, apr_interval_time_t timeout)
{
    /* Always OK to unset timeouts */
    if (timeout == -1) {
        thepipe->timeout = timeout;
        return APR_SUCCESS;
    }
    if (!thepipe->pipe) {
        return APR_ENOTIMPL;
    }
    if (timeout && !(thepipe->pOverlapped)) {
        /* Cannot be nonzero if a pipe was opened blocking
         */
        return APR_EINVAL;
    }
    thepipe->timeout = timeout;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_file_pipe_timeout_get(apr_file_t *thepipe, apr_interval_time_t *timeout)
{
    /* Always OK to get the timeout (even if it's unset ... -1) */
    *timeout = thepipe->timeout;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_file_pipe_create(apr_file_t **in, apr_file_t **out, apr_pool_t *p)
{
    /* Unix creates full blocking pipes. */
    return apr_create_nt_pipe(in, out, APR_FULL_BLOCK, p);
}

/* apr_create_nt_pipe()
 * An internal (for now) APR function used by apr_proc_create() 
 * when setting up pipes to communicate with the child process. 
 * apr_create_nt_pipe() allows setting the blocking mode of each end of 
 * the pipe when the pipe is created (rather than after the pipe is created). 
 * A pipe handle must be opened in full async i/o mode in order to 
 * emulate Unix non-blocking pipes with timeouts. 
 *
 * In general, we don't want to enable child side pipe handles for async i/o.
 * This prevents us from enabling both ends of the pipe for async i/o in 
 * apr_file_pipe_create.
 *
 * Why not use NamedPipes on NT which support setting pipe state to
 * non-blocking? On NT, even though you can set a pipe non-blocking, 
 * there is no clean way to set event driven non-zero timeouts (e.g select(),
 * WaitForSinglelObject, et. al. will not detect pipe i/o). On NT, you 
 * have to poll the pipe to detect i/o on a non-blocking pipe.
 */
apr_status_t apr_create_nt_pipe(apr_file_t **in, apr_file_t **out, 
                                apr_int32_t blocking_mode, 
                                apr_pool_t *p)
{
#ifdef _WIN32_WCE
    return APR_ENOTIMPL;
#else
    SECURITY_ATTRIBUTES sa;
    static unsigned long id = 0;
    DWORD dwPipeMode;
    DWORD dwOpenMode;
    char name[50];

    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;

    (*in) = (apr_file_t *)apr_pcalloc(p, sizeof(apr_file_t));
    (*in)->pool = p;
    (*in)->fname = NULL;
    (*in)->pipe = 1;
    (*in)->timeout = -1;
    (*in)->ungetchar = -1;
    (*in)->eof_hit = 0;
    (*in)->filePtr = 0;
    (*in)->bufpos = 0;
    (*in)->dataRead = 0;
    (*in)->direction = 0;
    (*in)->pOverlapped = NULL;
    (void) apr_pollset_create(&(*in)->pollset, 1, p, 0);

    (*out) = (apr_file_t *)apr_pcalloc(p, sizeof(apr_file_t));
    (*out)->pool = p;
    (*out)->fname = NULL;
    (*out)->pipe = 1;
    (*out)->timeout = -1;
    (*out)->ungetchar = -1;
    (*out)->eof_hit = 0;
    (*out)->filePtr = 0;
    (*out)->bufpos = 0;
    (*out)->dataRead = 0;
    (*out)->direction = 0;
    (*out)->pOverlapped = NULL;
    (void) apr_pollset_create(&(*out)->pollset, 1, p, 0);

    if (apr_os_level >= APR_WIN_NT) {
        /* Create the read end of the pipe */
        dwOpenMode = PIPE_ACCESS_INBOUND;
        if (blocking_mode == APR_WRITE_BLOCK /* READ_NONBLOCK */
               || blocking_mode == APR_FULL_NONBLOCK) {
            dwOpenMode |= FILE_FLAG_OVERLAPPED;
            (*in)->pOverlapped = (OVERLAPPED*) apr_pcalloc(p, sizeof(OVERLAPPED));
            (*in)->pOverlapped->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        }

        dwPipeMode = 0;

        sprintf(name, "\\\\.\\pipe\\apr-pipe-%u.%lu", getpid(), id++);

        (*in)->filehand = CreateNamedPipe(name,
                                          dwOpenMode,
                                          dwPipeMode,
                                          1,            //nMaxInstances,
                                          0,            //nOutBufferSize, 
                                          65536,        //nInBufferSize,                   
                                          1,            //nDefaultTimeOut,                
                                          &sa);

        /* Create the write end of the pipe */
        dwOpenMode = FILE_ATTRIBUTE_NORMAL;
        if (blocking_mode == APR_READ_BLOCK /* WRITE_NONBLOCK */
                || blocking_mode == APR_FULL_NONBLOCK) {
            dwOpenMode |= FILE_FLAG_OVERLAPPED;
            (*out)->pOverlapped = (OVERLAPPED*) apr_pcalloc(p, sizeof(OVERLAPPED));
            (*out)->pOverlapped->hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        }
        
        (*out)->filehand = CreateFile(name,
                                      GENERIC_WRITE,   // access mode
                                      0,               // share mode
                                      &sa,             // Security attributes
                                      OPEN_EXISTING,   // dwCreationDisposition
                                      dwOpenMode,      // Pipe attributes
                                      NULL);           // handle to template file
    }
    else {
        /* Pipes on Win9* are blocking. Live with it. */
        if (!CreatePipe(&(*in)->filehand, &(*out)->filehand, &sa, 65536)) {
            return apr_get_os_error();
        }
    }

    apr_pool_cleanup_register((*in)->pool, (void *)(*in), file_cleanup,
                        apr_pool_cleanup_null);
    apr_pool_cleanup_register((*out)->pool, (void *)(*out), file_cleanup,
                        apr_pool_cleanup_null);
    return APR_SUCCESS;
#endif /* _WIN32_WCE */
}


APR_DECLARE(apr_status_t) apr_file_namedpipe_create(const char *filename,
                                                    apr_fileperms_t perm,
                                                    apr_pool_t *pool)
{
    /* Not yet implemented, interface not suitable.
     * Win32 requires the named pipe to be *opened* at the time it's
     * created, and to do so, blocking or non blocking must be elected.
     */
    return APR_ENOTIMPL;
}


/* XXX: Problem; we need to choose between blocking and nonblocking based
 * on how *thefile was opened, and we don't have that information :-/
 * Hack; assume a blocking socket, since the most common use for the fn
 * would be to handle stdio-style or blocking pipes.  Win32 doesn't have
 * select() blocking for pipes anyways :(
 */
APR_DECLARE(apr_status_t) apr_os_pipe_put_ex(apr_file_t **file,
                                             apr_os_file_t *thefile,
                                             int register_cleanup,
                                             apr_pool_t *pool)
{
    (*file) = apr_pcalloc(pool, sizeof(apr_file_t));
    (*file)->pool = pool;
    (*file)->pipe = 1;
    (*file)->timeout = -1;
    (*file)->ungetchar = -1;
    (*file)->filehand = *thefile;
    (void) apr_pollset_create(&(*file)->pollset, 1, pool, 0);

    if (register_cleanup) {
        apr_pool_cleanup_register(pool, *file, file_cleanup,
                                  apr_pool_cleanup_null);
    }

    return APR_SUCCESS;
}


APR_DECLARE(apr_status_t) apr_os_pipe_put(apr_file_t **file,
                                          apr_os_file_t *thefile,
                                          apr_pool_t *pool)
{
    return apr_os_pipe_put_ex(file, thefile, 0, pool);
}
