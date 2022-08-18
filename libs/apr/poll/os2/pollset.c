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

#include "fspr.h"
#include "fspr_poll.h"
#include "fspr_arch_networkio.h"



struct fspr_pollset_t {
    fspr_pool_t *pool;
    fspr_uint32_t nelts;
    fspr_uint32_t nalloc;
    int *pollset;
    int num_read;
    int num_write;
    int num_except;
    int num_total;
    fspr_pollfd_t *query_set;
    fspr_pollfd_t *result_set;
};



APR_DECLARE(fspr_status_t) fspr_pollset_create(fspr_pollset_t **pollset,
                                             fspr_uint32_t size,
                                             fspr_pool_t *p,
                                             fspr_uint32_t flags)
{
    *pollset = fspr_palloc(p, sizeof(**pollset));
    (*pollset)->pool = p;
    (*pollset)->nelts = 0;
    (*pollset)->nalloc = size;
    (*pollset)->pollset = fspr_palloc(p, size * sizeof(int) * 3);
    (*pollset)->query_set = fspr_palloc(p, size * sizeof(fspr_pollfd_t));
    (*pollset)->result_set = fspr_palloc(p, size * sizeof(fspr_pollfd_t));
    (*pollset)->num_read = -1;
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_pollset_destroy(fspr_pollset_t *pollset)
{
    /* A no-op function for now.  If we later implement /dev/poll
     * support, we'll need to close the /dev/poll fd here
     */
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_pollset_add(fspr_pollset_t *pollset,
                                          const fspr_pollfd_t *descriptor)
{
    if (pollset->nelts == pollset->nalloc) {
        return APR_ENOMEM;
    }

    pollset->query_set[pollset->nelts] = *descriptor;

    if (descriptor->desc_type != APR_POLL_SOCKET) {
        return APR_EBADF;
    }

    pollset->nelts++;
    pollset->num_read = -1;
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_pollset_remove(fspr_pollset_t *pollset,
                                             const fspr_pollfd_t *descriptor)
{
    fspr_uint32_t i;

    for (i = 0; i < pollset->nelts; i++) {
        if (descriptor->desc.s == pollset->query_set[i].desc.s) {
            /* Found an instance of the fd: remove this and any other copies */
            fspr_uint32_t dst = i;
            fspr_uint32_t old_nelts = pollset->nelts;
            pollset->nelts--;

            for (i++; i < old_nelts; i++) {
                if (descriptor->desc.s == pollset->query_set[i].desc.s) {
                    pollset->nelts--;
                }
                else {
                    pollset->pollset[dst] = pollset->pollset[i];
                    pollset->query_set[dst] = pollset->query_set[i];
                    dst++;
                }
            }

            pollset->num_read = -1;
            return APR_SUCCESS;
        }
    }

    return APR_NOTFOUND;
}



static void make_pollset(fspr_pollset_t *pollset)
{
    int i;
    int pos = 0;

    pollset->num_read = 0;
    pollset->num_write = 0;
    pollset->num_except = 0;

    for (i = 0; i < pollset->nelts; i++) {
        if (pollset->query_set[i].reqevents & APR_POLLIN) {
            pollset->pollset[pos++] = pollset->query_set[i].desc.s->socketdes;
            pollset->num_read++;
        }
    }

    for (i = 0; i < pollset->nelts; i++) {
        if (pollset->query_set[i].reqevents & APR_POLLOUT) {
            pollset->pollset[pos++] = pollset->query_set[i].desc.s->socketdes;
            pollset->num_write++;
        }
    }

    for (i = 0; i < pollset->nelts; i++) {
        if (pollset->query_set[i].reqevents & APR_POLLPRI) {
            pollset->pollset[pos++] = pollset->query_set[i].desc.s->socketdes;
            pollset->num_except++;
        }
    }

    pollset->num_total = pollset->num_read + pollset->num_write + pollset->num_except;
}



APR_DECLARE(fspr_status_t) fspr_pollset_poll(fspr_pollset_t *pollset,
                                           fspr_interval_time_t timeout,
                                           fspr_int32_t *num,
                                           const fspr_pollfd_t **descriptors)
{
    int rv;
    fspr_uint32_t i;
    int *pollresult;
    int read_pos, write_pos, except_pos;

    if (pollset->num_read < 0) {
        make_pollset(pollset);
    }

    pollresult = alloca(sizeof(int) * pollset->num_total);
    memcpy(pollresult, pollset->pollset, sizeof(int) * pollset->num_total);
    (*num) = 0;

    if (timeout > 0) {
        timeout /= 1000;
    }

    rv = select(pollresult, pollset->num_read, pollset->num_write, pollset->num_except, timeout);

    if (rv < 0) {
        return APR_FROM_OS_ERROR(sock_errno());
    }

    if (rv == 0) {
        return APR_TIMEUP;
    }

    read_pos = 0;
    write_pos = pollset->num_read;
    except_pos = pollset->num_read + pollset->num_write;

    for (i = 0; i < pollset->nelts; i++) {
        int rtnevents = 0;

        if (pollset->query_set[i].reqevents & APR_POLLIN) {
            if (pollresult[read_pos++] != -1) {
                rtnevents |= APR_POLLIN;
            }
        }

        if (pollset->query_set[i].reqevents & APR_POLLOUT) {
            if (pollresult[write_pos++] != -1) {
                rtnevents |= APR_POLLOUT;
            }
        }

        if (pollset->query_set[i].reqevents & APR_POLLPRI) {
            if (pollresult[except_pos++] != -1) {
                rtnevents |= APR_POLLPRI;
            }
        }

        if (rtnevents) {
            pollset->result_set[*num] = pollset->query_set[i];
            pollset->result_set[*num].rtnevents = rtnevents;
            (*num)++;
        }
    }

    if (descriptors) {
        *descriptors = pollset->result_set;
    }

    return APR_SUCCESS;
}
