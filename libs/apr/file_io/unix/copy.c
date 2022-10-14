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

static fspr_status_t fspr_file_transfer_contents(const char *from_path,
                                               const char *to_path,
                                               fspr_int32_t flags,
                                               fspr_fileperms_t to_perms,
                                               fspr_pool_t *pool)
{
    fspr_file_t *s, *d;
    fspr_status_t status;
    fspr_finfo_t finfo;
    fspr_fileperms_t perms;

    /* Open source file. */
    status = fspr_file_open(&s, from_path, APR_READ, APR_OS_DEFAULT, pool);
    if (status)
        return status;

    /* Maybe get its permissions. */
    if (to_perms == APR_FILE_SOURCE_PERMS) {
        status = fspr_file_info_get(&finfo, APR_FINFO_PROT, s);
        if (status != APR_SUCCESS && status != APR_INCOMPLETE) {
            fspr_file_close(s);  /* toss any error */
            return status;
        }
        perms = finfo.protection;
    }
    else
        perms = to_perms;

    /* Open dest file. */
    status = fspr_file_open(&d, to_path, flags, perms, pool);
    if (status) {
        fspr_file_close(s);  /* toss any error */
        return status;
    }

    /* Copy bytes till the cows come home. */
    while (1) {
        char buf[BUFSIZ];
        fspr_size_t bytes_this_time = sizeof(buf);
        fspr_status_t read_err;
        fspr_status_t write_err;

        /* Read 'em. */
        read_err = fspr_file_read(s, buf, &bytes_this_time);
        if (read_err && !APR_STATUS_IS_EOF(read_err)) {
            fspr_file_close(s);  /* toss any error */
            fspr_file_close(d);  /* toss any error */
            return read_err;
        }

        /* Write 'em. */
        write_err = fspr_file_write_full(d, buf, bytes_this_time, NULL);
        if (write_err) {
            fspr_file_close(s);  /* toss any error */
            fspr_file_close(d);  /* toss any error */
            return write_err;
        }

        if (read_err && APR_STATUS_IS_EOF(read_err)) {
            status = fspr_file_close(s);
            if (status) {
                fspr_file_close(d);  /* toss any error */
                return status;
            }

            /* return the results of this close: an error, or success */
            return fspr_file_close(d);
        }
    }
    /* NOTREACHED */
}

APR_DECLARE(fspr_status_t) fspr_file_copy(const char *from_path,
                                        const char *to_path,
                                        fspr_fileperms_t perms,
                                        fspr_pool_t *pool)
{
    return fspr_file_transfer_contents(from_path, to_path,
                                      (APR_WRITE | APR_CREATE | APR_TRUNCATE),
                                      perms,
                                      pool);
}

APR_DECLARE(fspr_status_t) fspr_file_append(const char *from_path,
                                          const char *to_path,
                                          fspr_fileperms_t perms,
                                          fspr_pool_t *pool)
{
    return fspr_file_transfer_contents(from_path, to_path,
                                      (APR_WRITE | APR_CREATE | APR_APPEND),
                                      perms,
                                      pool);
}
