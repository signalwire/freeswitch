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

static fspr_status_t file_dup(fspr_file_t **new_file, 
                             fspr_file_t *old_file, fspr_pool_t *p,
                             int which_dup)
{
    int rv;
    
    if (which_dup == 2) {
        if ((*new_file) == NULL) {
            /* We can't dup2 unless we have a valid new_file */
            return APR_EINVAL;
        }
        rv = dup2(old_file->filedes, (*new_file)->filedes);
    } else {
        rv = dup(old_file->filedes);
    }

    if (rv == -1)
        return errno;
    
    if (which_dup == 1) {
        (*new_file) = (fspr_file_t *)fspr_pcalloc(p, sizeof(fspr_file_t));
        (*new_file)->pool = p;
        (*new_file)->filedes = rv;
    }

    (*new_file)->fname = fspr_pstrdup(p, old_file->fname);
    (*new_file)->buffered = old_file->buffered;

    /* If the existing socket in a dup2 is already buffered, we
     * have an existing and valid (hopefully) mutex, so we don't
     * want to create it again as we could leak!
     */
#if APR_HAS_THREADS
    if ((*new_file)->buffered && !(*new_file)->thlock && old_file->thlock) {
        fspr_thread_mutex_create(&((*new_file)->thlock),
                                APR_THREAD_MUTEX_DEFAULT, p);
    }
#endif
    /* As above, only create the buffer if we haven't already
     * got one.
     */
    if ((*new_file)->buffered && !(*new_file)->buffer) {
        (*new_file)->buffer = fspr_palloc(p, APR_FILE_BUFSIZE);
    }

    /* this is the way dup() works */
    (*new_file)->blocking = old_file->blocking; 

    /* make sure unget behavior is consistent */
    (*new_file)->ungetchar = old_file->ungetchar;

    /* fspr_file_dup2() retains the original cleanup, reflecting 
     * the existing inherit and nocleanup flags.  This means, 
     * that fspr_file_dup2() cannot be called against an fspr_file_t
     * already closed with fspr_file_close, because the expected
     * cleanup was already killed.
     */
    if (which_dup == 2) {
        return APR_SUCCESS;
    }

    /* fspr_file_dup() retains all old_file flags with the exceptions
     * of APR_INHERIT and APR_FILE_NOCLEANUP.
     * The user must call fspr_file_inherit_set() on the dupped 
     * fspr_file_t when desired.
     */
    (*new_file)->flags = old_file->flags
                       & ~(APR_INHERIT | APR_FILE_NOCLEANUP);

    fspr_pool_cleanup_register((*new_file)->pool, (void *)(*new_file),
                              fspr_unix_file_cleanup, 
                              fspr_unix_file_cleanup);
#ifndef WAITIO_USES_POLL
    /* Start out with no pollset.  fspr_wait_for_io_or_timeout() will
     * initialize the pollset if needed.
     */
    (*new_file)->pollset = NULL;
#endif
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_file_dup(fspr_file_t **new_file,
                                       fspr_file_t *old_file, fspr_pool_t *p)
{
    return file_dup(new_file, old_file, p, 1);
}

APR_DECLARE(fspr_status_t) fspr_file_dup2(fspr_file_t *new_file,
                                        fspr_file_t *old_file, fspr_pool_t *p)
{
    return file_dup(&new_file, old_file, p, 2);
}

APR_DECLARE(fspr_status_t) fspr_file_setaside(fspr_file_t **new_file,
                                            fspr_file_t *old_file,
                                            fspr_pool_t *p)
{
    *new_file = (fspr_file_t *)fspr_palloc(p, sizeof(fspr_file_t));
    memcpy(*new_file, old_file, sizeof(fspr_file_t));
    (*new_file)->pool = p;
    if (old_file->buffered) {
        (*new_file)->buffer = fspr_palloc(p, APR_FILE_BUFSIZE);
        if (old_file->direction == 1) {
            memcpy((*new_file)->buffer, old_file->buffer, old_file->bufpos);
        }
        else {
            memcpy((*new_file)->buffer, old_file->buffer, old_file->dataRead);
        }
#if APR_HAS_THREADS
        if (old_file->thlock) {
            fspr_thread_mutex_create(&((*new_file)->thlock),
                                    APR_THREAD_MUTEX_DEFAULT, p);
            fspr_thread_mutex_destroy(old_file->thlock);
        }
#endif /* APR_HAS_THREADS */
    }
    if (old_file->fname) {
        (*new_file)->fname = fspr_pstrdup(p, old_file->fname);
    }
    if (!(old_file->flags & APR_FILE_NOCLEANUP)) {
        fspr_pool_cleanup_register(p, (void *)(*new_file), 
                                  fspr_unix_file_cleanup,
                                  ((*new_file)->flags & APR_INHERIT)
                                     ? fspr_pool_cleanup_null
                                     : fspr_unix_file_cleanup);
    }

    old_file->filedes = -1;
    fspr_pool_cleanup_kill(old_file->pool, (void *)old_file,
                          fspr_unix_file_cleanup);
#ifndef WAITIO_USES_POLL
    (*new_file)->pollset = NULL;
#endif
    return APR_SUCCESS;
}
