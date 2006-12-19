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

#ifdef WIN32
/* POSIX defines 1024 for the FD_SETSIZE */
#define FD_SETSIZE 1024
#endif

#include "apr.h"
#include "apr_poll.h"
#include "apr_time.h"
#include "apr_portable.h"
#include "apr_arch_networkio.h"
#include "apr_arch_file_io.h"
#include "apr_arch_poll_private.h"

#ifdef POLL_USES_SELECT

APR_DECLARE(apr_status_t) apr_poll(apr_pollfd_t *aprset, int num,
                                   apr_int32_t *nsds,
                                   apr_interval_time_t timeout)
{
    fd_set readset, writeset, exceptset;
    int rv, i;
    int maxfd = -1;
    struct timeval tv, *tvptr;
#ifdef NETWARE
    apr_datatype_e set_type = APR_NO_DESC;
#endif

    if (timeout < 0) {
        tvptr = NULL;
    }
    else {
        tv.tv_sec = (long) apr_time_sec(timeout);
        tv.tv_usec = (long) apr_time_usec(timeout);
        tvptr = &tv;
    }

    FD_ZERO(&readset);
    FD_ZERO(&writeset);
    FD_ZERO(&exceptset);

    for (i = 0; i < num; i++) {
        apr_os_sock_t fd;

        aprset[i].rtnevents = 0;

        if (aprset[i].desc_type == APR_POLL_SOCKET) {
#ifdef NETWARE
            if (HAS_PIPES(set_type)) {
                return APR_EBADF;
            }
            else {
                set_type = APR_POLL_SOCKET;
            }
#endif
            fd = aprset[i].desc.s->socketdes;
        }
        else if (aprset[i].desc_type == APR_POLL_FILE) {
#if !APR_FILES_AS_SOCKETS
            return APR_EBADF;
#else
#ifdef NETWARE
            if (aprset[i].desc.f->is_pipe && !HAS_SOCKETS(set_type)) {
                set_type = APR_POLL_FILE;
            }
            else
                return APR_EBADF;
#endif /* NETWARE */

            fd = aprset[i].desc.f->filedes;

#endif /* APR_FILES_AS_SOCKETS */
        }
        else {
            break;
        }
#if !defined(WIN32) && !defined(NETWARE)        /* socket sets handled with array of handles */
        if (fd >= FD_SETSIZE) {
            /* XXX invent new error code so application has a clue */
            return APR_EBADF;
        }
#endif
        if (aprset[i].reqevents & APR_POLLIN) {
            FD_SET(fd, &readset);
        }
        if (aprset[i].reqevents & APR_POLLOUT) {
            FD_SET(fd, &writeset);
        }
        if (aprset[i].reqevents &
            (APR_POLLPRI | APR_POLLERR | APR_POLLHUP | APR_POLLNVAL)) {
            FD_SET(fd, &exceptset);
        }
        if ((int) fd > maxfd) {
            maxfd = (int) fd;
        }
    }

#ifdef NETWARE
    if (HAS_PIPES(set_type)) {
        rv = pipe_select(maxfd + 1, &readset, &writeset, &exceptset, tvptr);
    }
    else {
#endif

        rv = select(maxfd + 1, &readset, &writeset, &exceptset, tvptr);

#ifdef NETWARE
    }
#endif

    (*nsds) = rv;
    if ((*nsds) == 0) {
        return APR_TIMEUP;
    }
    if ((*nsds) < 0) {
        return apr_get_netos_error();
    }

    (*nsds) = 0;
    for (i = 0; i < num; i++) {
        apr_os_sock_t fd;

        if (aprset[i].desc_type == APR_POLL_SOCKET) {
            fd = aprset[i].desc.s->socketdes;
        }
        else if (aprset[i].desc_type == APR_POLL_FILE) {
#if !APR_FILES_AS_SOCKETS
            return APR_EBADF;
#else
            fd = aprset[i].desc.f->filedes;
#endif
        }
        else {
            break;
        }
        if (FD_ISSET(fd, &readset)) {
            aprset[i].rtnevents |= APR_POLLIN;
        }
        if (FD_ISSET(fd, &writeset)) {
            aprset[i].rtnevents |= APR_POLLOUT;
        }
        if (FD_ISSET(fd, &exceptset)) {
            aprset[i].rtnevents |= APR_POLLERR;
        }
        if (aprset[i].rtnevents) {
            (*nsds)++;
        }
    }

    return APR_SUCCESS;
}

#endif /* POLL_USES_SELECT */

#ifdef POLLSET_USES_SELECT

struct apr_pollset_t
{
    apr_pool_t *pool;

    apr_uint32_t nelts;
    apr_uint32_t nalloc;
    fd_set readset, writeset, exceptset;
    int maxfd;
    apr_pollfd_t *query_set;
    apr_pollfd_t *result_set;
#ifdef NETWARE
    int set_type;
#endif
};

APR_DECLARE(apr_status_t) apr_pollset_create(apr_pollset_t **pollset,
                                             apr_uint32_t size,
                                             apr_pool_t *p,
                                             apr_uint32_t flags)
{
    if (flags & APR_POLLSET_THREADSAFE) {                
        *pollset = NULL;
        return APR_ENOTIMPL;
    }
#ifdef FD_SETSIZE
    if (size > FD_SETSIZE) {
        *pollset = NULL;
        return APR_EINVAL;
    }
#endif
    *pollset = apr_palloc(p, sizeof(**pollset));
    (*pollset)->nelts = 0;
    (*pollset)->nalloc = size;
    (*pollset)->pool = p;
    FD_ZERO(&((*pollset)->readset));
    FD_ZERO(&((*pollset)->writeset));
    FD_ZERO(&((*pollset)->exceptset));
    (*pollset)->maxfd = 0;
#ifdef NETWARE
    (*pollset)->set_type = APR_NO_DESC;
#endif
    (*pollset)->query_set = apr_palloc(p, size * sizeof(apr_pollfd_t));
    (*pollset)->result_set = apr_palloc(p, size * sizeof(apr_pollfd_t));

    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_pollset_destroy(apr_pollset_t * pollset)
{
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_pollset_add(apr_pollset_t *pollset,
                                          const apr_pollfd_t *descriptor)
{
    apr_os_sock_t fd;

    if (pollset->nelts == pollset->nalloc) {
        return APR_ENOMEM;
    }

    pollset->query_set[pollset->nelts] = *descriptor;

    if (descriptor->desc_type == APR_POLL_SOCKET) {
#ifdef NETWARE
        /* NetWare can't handle mixed descriptor types in select() */
        if (HAS_PIPES(pollset->set_type)) {
            return APR_EBADF;
        }
        else {
            pollset->set_type = APR_POLL_SOCKET;
        }
#endif
        fd = descriptor->desc.s->socketdes;
    }
    else {
#if !APR_FILES_AS_SOCKETS
        return APR_EBADF;
#else
#ifdef NETWARE
        /* NetWare can't handle mixed descriptor types in select() */
        if (descriptor->desc.f->is_pipe && !HAS_SOCKETS(pollset->set_type)) {
            pollset->set_type = APR_POLL_FILE;
            fd = descriptor->desc.f->filedes;
        }
        else {
            return APR_EBADF;
        }
#else
        fd = descriptor->desc.f->filedes;
#endif
#endif
    }
#if !defined(WIN32) && !defined(NETWARE)        /* socket sets handled with array of handles */
    if (fd >= FD_SETSIZE) {
        /* XXX invent new error code so application has a clue */
        return APR_EBADF;
    }
#endif
    if (descriptor->reqevents & APR_POLLIN) {
        FD_SET(fd, &(pollset->readset));
    }
    if (descriptor->reqevents & APR_POLLOUT) {
        FD_SET(fd, &(pollset->writeset));
    }
    if (descriptor->reqevents &
        (APR_POLLPRI | APR_POLLERR | APR_POLLHUP | APR_POLLNVAL)) {
        FD_SET(fd, &(pollset->exceptset));
    }
    if ((int) fd > pollset->maxfd) {
        pollset->maxfd = (int) fd;
    }
    pollset->nelts++;
    return APR_SUCCESS;
}

APR_DECLARE(apr_status_t) apr_pollset_remove(apr_pollset_t * pollset,
                                             const apr_pollfd_t * descriptor)
{
    apr_uint32_t i;
    apr_os_sock_t fd;

    if (descriptor->desc_type == APR_POLL_SOCKET) {
        fd = descriptor->desc.s->socketdes;
    }
    else {
#if !APR_FILES_AS_SOCKETS
        return APR_EBADF;
#else
        fd = descriptor->desc.f->filedes;
#endif
    }

    for (i = 0; i < pollset->nelts; i++) {
        if (descriptor->desc.s == pollset->query_set[i].desc.s) {
            /* Found an instance of the fd: remove this and any other copies */
            apr_uint32_t dst = i;
            apr_uint32_t old_nelts = pollset->nelts;
            pollset->nelts--;
            for (i++; i < old_nelts; i++) {
                if (descriptor->desc.s == pollset->query_set[i].desc.s) {
                    pollset->nelts--;
                }
                else {
                    pollset->query_set[dst] = pollset->query_set[i];
                    dst++;
                }
            }
            FD_CLR(fd, &(pollset->readset));
            FD_CLR(fd, &(pollset->writeset));
            FD_CLR(fd, &(pollset->exceptset));
            if (((int) fd == pollset->maxfd) && (pollset->maxfd > 0)) {
                pollset->maxfd--;
            }
            return APR_SUCCESS;
        }
    }

    return APR_NOTFOUND;
}

APR_DECLARE(apr_status_t) apr_pollset_poll(apr_pollset_t *pollset,
                                           apr_interval_time_t timeout,
                                           apr_int32_t *num,
                                           const apr_pollfd_t **descriptors)
{
    int rv;
    apr_uint32_t i, j;
    struct timeval tv, *tvptr;
    fd_set readset, writeset, exceptset;

    if (timeout < 0) {
        tvptr = NULL;
    }
    else {
        tv.tv_sec = (long) apr_time_sec(timeout);
        tv.tv_usec = (long) apr_time_usec(timeout);
        tvptr = &tv;
    }

    memcpy(&readset, &(pollset->readset), sizeof(fd_set));
    memcpy(&writeset, &(pollset->writeset), sizeof(fd_set));
    memcpy(&exceptset, &(pollset->exceptset), sizeof(fd_set));

#ifdef NETWARE
    if (HAS_PIPES(pollset->set_type)) {
        rv = pipe_select(pollset->maxfd + 1, &readset, &writeset, &exceptset,
                         tvptr);
    }
    else
#endif
        rv = select(pollset->maxfd + 1, &readset, &writeset, &exceptset,
                    tvptr);

    (*num) = rv;
    if (rv < 0) {
        return apr_get_netos_error();
    }
    if (rv == 0) {
        return APR_TIMEUP;
    }
    j = 0;
    for (i = 0; i < pollset->nelts; i++) {
        apr_os_sock_t fd;
        if (pollset->query_set[i].desc_type == APR_POLL_SOCKET) {
            fd = pollset->query_set[i].desc.s->socketdes;
        }
        else {
#if !APR_FILES_AS_SOCKETS
            return APR_EBADF;
#else
            fd = pollset->query_set[i].desc.f->filedes;
#endif
        }
        if (FD_ISSET(fd, &readset) || FD_ISSET(fd, &writeset) ||
            FD_ISSET(fd, &exceptset)) {
            pollset->result_set[j] = pollset->query_set[i];
            pollset->result_set[j].rtnevents = 0;
            if (FD_ISSET(fd, &readset)) {
                pollset->result_set[j].rtnevents |= APR_POLLIN;
            }
            if (FD_ISSET(fd, &writeset)) {
                pollset->result_set[j].rtnevents |= APR_POLLOUT;
            }
            if (FD_ISSET(fd, &exceptset)) {
                pollset->result_set[j].rtnevents |= APR_POLLERR;
            }
            j++;
        }
    }
    (*num) = j;

    if (descriptors)
        *descriptors = pollset->result_set;
    return APR_SUCCESS;
}

#endif /* POLLSET_USES_SELECT */
