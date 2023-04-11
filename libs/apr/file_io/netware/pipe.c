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

#include <stdio.h>
#include <nks/fsio.h>
#include <nks/errno.h>

#include "fspr_arch_file_io.h"
#include "fspr_strings.h"
#include "fspr_portable.h"
#include "fspr_arch_inherit.h"

static fspr_status_t pipeblock(fspr_file_t *thepipe)
{
#ifdef USE_FLAGS
    int				err;
	unsigned long	flags;

	if (fcntl(thepipe->filedes, F_GETFL, &flags) != -1)
	{
		flags &= ~FNDELAY;
		fcntl(thepipe->filedes, F_SETFL, flags);
	}
#else
        errno = 0;
		fcntl(thepipe->filedes, F_SETFL, 0);
#endif

    if (errno)
        return errno;

    thepipe->blocking = BLK_ON;
    return APR_SUCCESS;
}

static fspr_status_t pipenonblock(fspr_file_t *thepipe)
{
#ifdef USE_FLAGS
	int				err;
	unsigned long	flags;

    errno = 0;
	if (fcntl(thepipe->filedes, F_GETFL, &flags) != -1)
	{
		flags |= FNDELAY;
		fcntl(thepipe->filedes, F_SETFL, flags);
	}
#else
        errno = 0;
		fcntl(thepipe->filedes, F_SETFL, FNDELAY);
#endif

    if (errno)
        return errno;

    thepipe->blocking = BLK_OFF;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_file_pipe_timeout_set(fspr_file_t *thepipe, fspr_interval_time_t timeout)
{
    if (thepipe->is_pipe == 1) {
        thepipe->timeout = timeout;
        if (timeout >= 0) {
            if (thepipe->blocking != BLK_OFF) { /* blocking or unknown state */
                return pipenonblock(thepipe);
            }
        }
        else {
            if (thepipe->blocking != BLK_ON) { /* non-blocking or unknown state */
                return pipeblock(thepipe);
            }
        }
        return APR_SUCCESS;
    }
    return APR_EINVAL;
}

APR_DECLARE(fspr_status_t) fspr_file_pipe_timeout_get(fspr_file_t *thepipe, fspr_interval_time_t *timeout)
{
    if (thepipe->is_pipe == 1) {
        *timeout = thepipe->timeout;
        return APR_SUCCESS;
    }
    return APR_EINVAL;
}

APR_DECLARE(fspr_status_t) fspr_os_pipe_put_ex(fspr_file_t **file,
                                             fspr_os_file_t *thefile,
                                             int register_cleanup,
                                             fspr_pool_t *pool)
{
    int *dafile = thefile;
    
    (*file) = fspr_pcalloc(pool, sizeof(fspr_file_t));
    (*file)->pool = pool;
    (*file)->eof_hit = 0;
    (*file)->is_pipe = 1;
    (*file)->blocking = BLK_UNKNOWN; /* app needs to make a timeout call */
    (*file)->timeout = -1;
    (*file)->ungetchar = -1; /* no char avail */
    (*file)->filedes = *dafile;
    if (!register_cleanup) {
        (*file)->flags = APR_FILE_NOCLEANUP;
    }
    (*file)->buffered = 0;
#if APR_HAS_THREADS
    (*file)->thlock = NULL;
#endif
    if (register_cleanup) {
        fspr_pool_cleanup_register((*file)->pool, (void *)(*file),
                                  fspr_unix_file_cleanup,
                                  fspr_pool_cleanup_null);
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_pipe_put(fspr_file_t **file,
                                          fspr_os_file_t *thefile,
                                          fspr_pool_t *pool)
{
    return fspr_os_pipe_put_ex(file, thefile, 0, pool);
}

APR_DECLARE(fspr_status_t) fspr_file_pipe_create(fspr_file_t **in, fspr_file_t **out, fspr_pool_t *pool)
{
	int     	filedes[2];
	int 		err;

    if (pipe(filedes) == -1) {
        return errno;
    }

    (*in) = (fspr_file_t *)fspr_pcalloc(pool, sizeof(fspr_file_t));
    (*out) = (fspr_file_t *)fspr_pcalloc(pool, sizeof(fspr_file_t));

    (*in)->pool     =
    (*out)->pool    = pool;
    (*in)->filedes   = filedes[0];
    (*out)->filedes  = filedes[1];
    (*in)->flags     = APR_INHERIT;
    (*out)->flags    = APR_INHERIT;
    (*in)->is_pipe      =
    (*out)->is_pipe     = 1;
    (*out)->fname    = 
    (*in)->fname     = NULL;
    (*in)->buffered  =
    (*out)->buffered = 0;
    (*in)->blocking  =
    (*out)->blocking = BLK_ON;
    (*in)->timeout   =
    (*out)->timeout  = -1;
    (*in)->ungetchar = -1;
    (*in)->thlock    =
    (*out)->thlock   = NULL;
    (void) fspr_pollset_create(&(*in)->pollset, 1, pool, 0);
    (void) fspr_pollset_create(&(*out)->pollset, 1, pool, 0);

    fspr_pool_cleanup_register((*in)->pool, (void *)(*in), fspr_unix_file_cleanup,
                         fspr_pool_cleanup_null);
    fspr_pool_cleanup_register((*out)->pool, (void *)(*out), fspr_unix_file_cleanup,
                         fspr_pool_cleanup_null);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_file_namedpipe_create(const char *filename, 
                                                    fspr_fileperms_t perm, fspr_pool_t *pool)
{
    return APR_ENOTIMPL;
} 

    

