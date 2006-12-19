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
#include <string.h>
#include "apr_arch_inherit.h"

APR_DECLARE(apr_status_t) apr_file_dup(apr_file_t **new_file,
                                       apr_file_t *old_file, apr_pool_t *p)
{
#ifdef _WIN32_WCE
    return APR_ENOTIMPL;
#else
    HANDLE hproc = GetCurrentProcess();
    HANDLE newhand = NULL;

    if (!DuplicateHandle(hproc, old_file->filehand, 
                         hproc, &newhand, 0, FALSE, 
                         DUPLICATE_SAME_ACCESS)) {
        return apr_get_os_error();
    }

    (*new_file) = (apr_file_t *) apr_pcalloc(p, sizeof(apr_file_t));
    (*new_file)->filehand = newhand;
    (*new_file)->flags = old_file->flags & ~APR_INHERIT;
    (*new_file)->pool = p;
    (*new_file)->fname = apr_pstrdup(p, old_file->fname);
    (*new_file)->append = old_file->append;
    (*new_file)->buffered = FALSE;
    (*new_file)->ungetchar = old_file->ungetchar;

#if APR_HAS_THREADS
    if (old_file->mutex) {
        apr_thread_mutex_create(&((*new_file)->mutex),
                                APR_THREAD_MUTEX_DEFAULT, p);
    }
#endif

    apr_pool_cleanup_register((*new_file)->pool, (void *)(*new_file), file_cleanup,
                        apr_pool_cleanup_null);

    /* Create a pollset with room for one descriptor. */
    /* ### check return codes */
    (void) apr_pollset_create(&(*new_file)->pollset, 1, p, 0);

    return APR_SUCCESS;
#endif /* !defined(_WIN32_WCE) */
}

#define stdin_handle 0x01
#define stdout_handle 0x02
#define stderr_handle 0x04

APR_DECLARE(apr_status_t) apr_file_dup2(apr_file_t *new_file,
                                        apr_file_t *old_file, apr_pool_t *p)
{
#ifdef _WIN32_WCE
    return APR_ENOTIMPL;
#else
    DWORD stdhandle = 0;
    HANDLE hproc = GetCurrentProcess();
    HANDLE newhand = NULL;
    apr_int32_t newflags;

    /* dup2 is not supported literaly with native Windows handles.
     * We can, however, emulate dup2 for the standard i/o handles,
     * and close and replace other handles with duped handles.
     * The os_handle will change, however.
     */
    if (new_file->filehand == GetStdHandle(STD_ERROR_HANDLE)) {
        stdhandle |= stderr_handle;
    }
    if (new_file->filehand == GetStdHandle(STD_OUTPUT_HANDLE)) {
        stdhandle |= stdout_handle;
    }
    if (new_file->filehand == GetStdHandle(STD_INPUT_HANDLE)) {
        stdhandle |= stdin_handle;
    }

    if (stdhandle) {
        if (!DuplicateHandle(hproc, old_file->filehand, 
                             hproc, &newhand, 0,
                             TRUE, DUPLICATE_SAME_ACCESS)) {
            return apr_get_os_error();
        }
        if (((stdhandle & stderr_handle) && !SetStdHandle(STD_ERROR_HANDLE, newhand)) ||
            ((stdhandle & stdout_handle) && !SetStdHandle(STD_OUTPUT_HANDLE, newhand)) ||
            ((stdhandle & stdin_handle) && !SetStdHandle(STD_INPUT_HANDLE, newhand))) {
            return apr_get_os_error();
        }
        newflags = old_file->flags | APR_INHERIT;
    }
    else {
        if (!DuplicateHandle(hproc, old_file->filehand, 
                             hproc, &newhand, 0,
                             FALSE, DUPLICATE_SAME_ACCESS)) {
            return apr_get_os_error();
        }
        newflags = old_file->flags & ~APR_INHERIT;
    }

    if (new_file->filehand && (new_file->filehand != INVALID_HANDLE_VALUE)) {
        CloseHandle(new_file->filehand);
    }

    new_file->flags = newflags;
    new_file->filehand = newhand;
    new_file->fname = apr_pstrdup(new_file->pool, old_file->fname);
    new_file->append = old_file->append;
    new_file->buffered = FALSE;
    new_file->ungetchar = old_file->ungetchar;

#if APR_HAS_THREADS
    if (old_file->mutex) {
        apr_thread_mutex_create(&(new_file->mutex),
                                APR_THREAD_MUTEX_DEFAULT, p);
    }
#endif

    return APR_SUCCESS;
#endif /* !defined(_WIN32_WCE) */
}

APR_DECLARE(apr_status_t) apr_file_setaside(apr_file_t **new_file,
                                            apr_file_t *old_file,
                                            apr_pool_t *p)
{
    *new_file = (apr_file_t *)apr_palloc(p, sizeof(apr_file_t));
    memcpy(*new_file, old_file, sizeof(apr_file_t));
    (*new_file)->pool = p;
    if (old_file->buffered) {
        (*new_file)->buffer = apr_palloc(p, APR_FILE_BUFSIZE);
        if (old_file->direction == 1) {
            memcpy((*new_file)->buffer, old_file->buffer, old_file->bufpos);
        }
        else {
            memcpy((*new_file)->buffer, old_file->buffer, old_file->dataRead);
        }
    }
    if (old_file->mutex) {
        apr_thread_mutex_create(&((*new_file)->mutex),
                                APR_THREAD_MUTEX_DEFAULT, p);
        apr_thread_mutex_destroy(old_file->mutex);
    }
    if (old_file->fname) {
        (*new_file)->fname = apr_pstrdup(p, old_file->fname);
    }
    if (!(old_file->flags & APR_FILE_NOCLEANUP)) {
        apr_pool_cleanup_register(p, (void *)(*new_file), 
                                  file_cleanup,
                                  file_cleanup);
    }

    old_file->filehand = INVALID_HANDLE_VALUE;
    apr_pool_cleanup_kill(old_file->pool, (void *)old_file,
                          file_cleanup);

    /* Create a pollset with room for one descriptor. */
    /* ### check return codes */
    (void) apr_pollset_create(&(*new_file)->pollset, 1, p, 0);

    return APR_SUCCESS;
}
