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

#include "fspr_private.h"
#include "fspr_file_io.h" /* prototype of fspr_mkstemp() */
#include "fspr_strings.h" /* prototype of fspr_mkstemp() */
#include "fspr_arch_file_io.h" /* prototype of fspr_mkstemp() */
#include "fspr_portable.h" /* for fspr_os_file_put() */

#include <stdlib.h> /* for mkstemp() - Single Unix */

APR_DECLARE(fspr_status_t) fspr_file_mktemp(fspr_file_t **fp, char *template, fspr_int32_t flags, fspr_pool_t *p)
{
    int fd;
    fspr_status_t rv;

    flags = (!flags) ? APR_CREATE | APR_READ | APR_WRITE |  
                       APR_DELONCLOSE : flags & ~APR_EXCL;

    fd = mkstemp(template);
    if (fd == -1) {
        return errno;
    }
    /* We need to reopen the file to get rid of the o_excl flag.
     * Otherwise file locking will not allow the file to be shared.
     */
    close(fd);
    if ((rv = fspr_file_open(fp, template, flags|APR_FILE_NOCLEANUP,
                            APR_UREAD | APR_UWRITE, p)) == APR_SUCCESS) {

        fspr_pool_cleanup_register((*fp)->pool, (void *)(*fp),
                                  fspr_unix_file_cleanup, fspr_unix_file_cleanup);
    }

    return rv;
}

