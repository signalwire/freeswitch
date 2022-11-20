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

#define INCL_DOSERRORS
#include "fspr_arch_file_io.h"
#include "fspr_file_io.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "fspr_portable.h"
#include <string.h>
#include <process.h>

APR_DECLARE(fspr_status_t) fspr_file_pipe_create(fspr_file_t **in, fspr_file_t **out, fspr_pool_t *pool)
{
    ULONG filedes[2];
    ULONG rc, action;
    static int id = 0;
    char pipename[50];

    sprintf(pipename, "/pipe/%d.%d", getpid(), id++);
    rc = DosCreateNPipe(pipename, filedes, NP_ACCESS_INBOUND, NP_NOWAIT|1, 4096, 4096, 0);

    if (rc)
        return APR_FROM_OS_ERROR(rc);

    rc = DosConnectNPipe(filedes[0]);

    if (rc && rc != ERROR_PIPE_NOT_CONNECTED) {
        DosClose(filedes[0]);
        return APR_FROM_OS_ERROR(rc);
    }

    rc = DosOpen (pipename, filedes+1, &action, 0, FILE_NORMAL,
                  OPEN_ACTION_OPEN_IF_EXISTS | OPEN_ACTION_FAIL_IF_NEW,
                  OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYREADWRITE,
                  NULL);

    if (rc) {
        DosClose(filedes[0]);
        return APR_FROM_OS_ERROR(rc);
    }

    (*in) = (fspr_file_t *)fspr_palloc(pool, sizeof(fspr_file_t));
    rc = DosCreateEventSem(NULL, &(*in)->pipeSem, DC_SEM_SHARED, FALSE);

    if (rc) {
        DosClose(filedes[0]);
        DosClose(filedes[1]);
        return APR_FROM_OS_ERROR(rc);
    }

    rc = DosSetNPipeSem(filedes[0], (HSEM)(*in)->pipeSem, 1);

    if (!rc) {
        rc = DosSetNPHState(filedes[0], NP_WAIT);
    }

    if (rc) {
        DosClose(filedes[0]);
        DosClose(filedes[1]);
        DosCloseEventSem((*in)->pipeSem);
        return APR_FROM_OS_ERROR(rc);
    }

    (*in)->pool = pool;
    (*in)->filedes = filedes[0];
    (*in)->fname = fspr_pstrdup(pool, pipename);
    (*in)->isopen = TRUE;
    (*in)->buffered = FALSE;
    (*in)->flags = 0;
    (*in)->pipe = 1;
    (*in)->timeout = -1;
    (*in)->blocking = BLK_ON;
    fspr_pool_cleanup_register(pool, *in, fspr_file_cleanup, fspr_pool_cleanup_null);

    (*out) = (fspr_file_t *)fspr_palloc(pool, sizeof(fspr_file_t));
    (*out)->pool = pool;
    (*out)->filedes = filedes[1];
    (*out)->fname = fspr_pstrdup(pool, pipename);
    (*out)->isopen = TRUE;
    (*out)->buffered = FALSE;
    (*out)->flags = 0;
    (*out)->pipe = 1;
    (*out)->timeout = -1;
    (*out)->blocking = BLK_ON;
    fspr_pool_cleanup_register(pool, *out, fspr_file_cleanup, fspr_pool_cleanup_null);

    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_file_namedpipe_create(const char *filename, fspr_fileperms_t perm, fspr_pool_t *pool)
{
    /* Not yet implemented, interface not suitable */
    return APR_ENOTIMPL;
} 

 

APR_DECLARE(fspr_status_t) fspr_file_pipe_timeout_set(fspr_file_t *thepipe, fspr_interval_time_t timeout)
{
    if (thepipe->pipe == 1) {
        thepipe->timeout = timeout;

        if (thepipe->timeout >= 0) {
            if (thepipe->blocking != BLK_OFF) {
                thepipe->blocking = BLK_OFF;
                return APR_FROM_OS_ERROR(DosSetNPHState(thepipe->filedes, NP_NOWAIT));
            }
        }
        else if (thepipe->timeout == -1) {
            if (thepipe->blocking != BLK_ON) {
                thepipe->blocking = BLK_ON;
                return APR_FROM_OS_ERROR(DosSetNPHState(thepipe->filedes, NP_WAIT));
            }
        }
    }
    return APR_EINVAL;
}



APR_DECLARE(fspr_status_t) fspr_file_pipe_timeout_get(fspr_file_t *thepipe, fspr_interval_time_t *timeout)
{
    if (thepipe->pipe == 1) {
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
    (*file) = fspr_pcalloc(pool, sizeof(fspr_file_t));
    (*file)->pool = pool;
    (*file)->isopen = TRUE;
    (*file)->pipe = 1;
    (*file)->blocking = BLK_UNKNOWN; /* app needs to make a timeout call */
    (*file)->timeout = -1;
    (*file)->filedes = *thefile;

    if (register_cleanup) {
        fspr_pool_cleanup_register(pool, *file, fspr_file_cleanup,
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
