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

#include "fspr_file_io.h"


APR_DECLARE(fspr_status_t) fspr_file_read_full(fspr_file_t *thefile, void *buf,
                                             fspr_size_t nbytes,
                                             fspr_size_t *bytes_read)
{
    fspr_status_t status;
    fspr_size_t total_read = 0;

    do {
	fspr_size_t amt = nbytes;

	status = fspr_file_read(thefile, buf, &amt);
	buf = (char *)buf + amt;
        nbytes -= amt;
        total_read += amt;
    } while (status == APR_SUCCESS && nbytes > 0);

    if (bytes_read != NULL)
        *bytes_read = total_read;

    return status;
}

APR_DECLARE(fspr_status_t) fspr_file_write_full(fspr_file_t *thefile,
                                              const void *buf,
                                              fspr_size_t nbytes,
                                              fspr_size_t *bytes_written)
{
    fspr_status_t status;
    fspr_size_t total_written = 0;

    do {
	fspr_size_t amt = nbytes;

	status = fspr_file_write(thefile, buf, &amt);
	buf = (char *)buf + amt;
        nbytes -= amt;
        total_written += amt;
    } while (status == APR_SUCCESS && nbytes > 0);

    if (bytes_written != NULL)
        *bytes_written = total_written;

    return status;
}

APR_DECLARE(fspr_status_t) fspr_file_writev_full(fspr_file_t *thefile,
                                               const struct iovec *vec,
                                               fspr_size_t nvec,
                                               fspr_size_t *bytes_written)
{
    fspr_status_t rv = APR_SUCCESS;
    fspr_size_t i;
    fspr_size_t amt = 0;
    fspr_size_t total = 0;

    for (i = 0; i < nvec && rv == APR_SUCCESS; i++) {
        rv = fspr_file_write_full(thefile, vec[i].iov_base, 
                                 vec[i].iov_len, &amt);
        total += amt;
    }

    if (bytes_written != NULL)
        *bytes_written = total;

    return rv;
}
