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
#include <string.h>
#include "fspr_arch_inherit.h"

static fspr_status_t file_dup(fspr_file_t **new_file, fspr_file_t *old_file, fspr_pool_t *p)
{
    int rv;
    fspr_file_t *dup_file;

    if (*new_file == NULL) {
        dup_file = (fspr_file_t *)fspr_palloc(p, sizeof(fspr_file_t));

        if (dup_file == NULL) {
            return APR_ENOMEM;
        }

        dup_file->filedes = -1;
    } else {
      dup_file = *new_file;
    }

    dup_file->pool = p;
    rv = DosDupHandle(old_file->filedes, &dup_file->filedes);

    if (rv) {
        return APR_FROM_OS_ERROR(rv);
    }

    dup_file->fname = fspr_pstrdup(dup_file->pool, old_file->fname);
    dup_file->buffered = old_file->buffered;
    dup_file->isopen = old_file->isopen;
    dup_file->flags = old_file->flags & ~APR_INHERIT;
    /* TODO - dup pipes correctly */
    dup_file->pipe = old_file->pipe;

    if (*new_file == NULL) {
        fspr_pool_cleanup_register(dup_file->pool, dup_file, fspr_file_cleanup,
                            fspr_pool_cleanup_null);
        *new_file = dup_file;
    }

    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_file_dup(fspr_file_t **new_file, fspr_file_t *old_file, fspr_pool_t *p)
{
  if (*new_file) {
      fspr_file_close(*new_file);
      (*new_file)->filedes = -1;
  }

  return file_dup(new_file, old_file, p);
}



APR_DECLARE(fspr_status_t) fspr_file_dup2(fspr_file_t *new_file, fspr_file_t *old_file, fspr_pool_t *p)
{
  return file_dup(&new_file, old_file, p);
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

        if (old_file->mutex) {
            fspr_thread_mutex_create(&((*new_file)->mutex),
                                    APR_THREAD_MUTEX_DEFAULT, p);
            fspr_thread_mutex_destroy(old_file->mutex);
        }
    }

    if (old_file->fname) {
        (*new_file)->fname = fspr_pstrdup(p, old_file->fname);
    }

    if (!(old_file->flags & APR_FILE_NOCLEANUP)) {
        fspr_pool_cleanup_register(p, (void *)(*new_file), 
                                  fspr_file_cleanup,
                                  fspr_file_cleanup);
    }

    old_file->filedes = -1;
    fspr_pool_cleanup_kill(old_file->pool, (void *)old_file,
                          fspr_file_cleanup);

    return APR_SUCCESS;
}
