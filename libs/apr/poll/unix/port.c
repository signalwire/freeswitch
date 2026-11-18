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

#include "fspr_arch_poll_private.h"

#ifdef POLLSET_USES_PORT

static fspr_int16_t get_event(fspr_int16_t event)
{
    fspr_int16_t rv = 0;

    if (event & APR_POLLIN)
        rv |= POLLIN;
    if (event & APR_POLLPRI)
        rv |= POLLPRI;
    if (event & APR_POLLOUT)
        rv |= POLLOUT;
    if (event & APR_POLLERR)
        rv |= POLLERR;
    if (event & APR_POLLHUP)
        rv |= POLLHUP;
    if (event & APR_POLLNVAL)
        rv |= POLLNVAL;

    return rv;
}

static fspr_int16_t get_revent(fspr_int16_t event)
{
    fspr_int16_t rv = 0;

    if (event & POLLIN)
        rv |= APR_POLLIN;
    if (event & POLLPRI)
        rv |= APR_POLLPRI;
    if (event & POLLOUT)
        rv |= APR_POLLOUT;
    if (event & POLLERR)
        rv |= APR_POLLERR;
    if (event & POLLHUP)
        rv |= APR_POLLHUP;
    if (event & POLLNVAL)
        rv |= APR_POLLNVAL;

    return rv;
}


struct fspr_pollset_t
{
    fspr_pool_t *pool;
    fspr_uint32_t nelts;
    fspr_uint32_t nalloc;
    int port_fd;
    port_event_t *port_set;
    fspr_pollfd_t *result_set;
    fspr_uint32_t flags;
#if APR_HAS_THREADS
    /* A thread mutex to protect operations on the rings */
    fspr_thread_mutex_t *ring_lock;
#endif
    /* A ring containing all of the pollfd_t that are active */
    APR_RING_HEAD(pfd_query_ring_t, pfd_elem_t) query_ring;
    APR_RING_HEAD(pfd_add_ring_t, pfd_elem_t) add_ring;
    /* A ring of pollfd_t that have been used, and then _remove'd */
    APR_RING_HEAD(pfd_free_ring_t, pfd_elem_t) free_ring;
    /* A ring of pollfd_t where rings that have been _remove'd but
       might still be inside a _poll */
    APR_RING_HEAD(pfd_dead_ring_t, pfd_elem_t) dead_ring;
};

static fspr_status_t backend_cleanup(void *p_)
{
    fspr_pollset_t *pollset = (fspr_pollset_t *) p_;
    close(pollset->port_fd);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_pollset_create(fspr_pollset_t **pollset,
                                             fspr_uint32_t size,
                                             fspr_pool_t *p,
                                             fspr_uint32_t flags)
{
    fspr_status_t rv = APR_SUCCESS;
    *pollset = fspr_palloc(p, sizeof(**pollset));
#if APR_HAS_THREADS
    if (flags & APR_POLLSET_THREADSAFE &&
        ((rv = fspr_thread_mutex_create(&(*pollset)->ring_lock,
                                       APR_THREAD_MUTEX_DEFAULT,
                                       p) != APR_SUCCESS))) {
        *pollset = NULL;
        return rv;
    }
#else
    if (flags & APR_POLLSET_THREADSAFE) {
        *pollset = NULL;
        return APR_ENOTIMPL;
    }
#endif
    (*pollset)->nelts = 0;
    (*pollset)->nalloc = size;
    (*pollset)->flags = flags;
    (*pollset)->pool = p;

    (*pollset)->port_set = fspr_palloc(p, size * sizeof(port_event_t));

    (*pollset)->port_fd = port_create();

    if ((*pollset)->port_fd < 0) {
        return APR_ENOMEM;
    }

    fspr_pool_cleanup_register(p, (void *) (*pollset), backend_cleanup,
                              fspr_pool_cleanup_null);

    (*pollset)->result_set = fspr_palloc(p, size * sizeof(fspr_pollfd_t));

    APR_RING_INIT(&(*pollset)->query_ring, pfd_elem_t, link);
    APR_RING_INIT(&(*pollset)->add_ring, pfd_elem_t, link);
    APR_RING_INIT(&(*pollset)->free_ring, pfd_elem_t, link);
    APR_RING_INIT(&(*pollset)->dead_ring, pfd_elem_t, link);

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_pollset_destroy(fspr_pollset_t *pollset)
{
    return fspr_pool_cleanup_run(pollset->pool, pollset, backend_cleanup);
}

APR_DECLARE(fspr_status_t) fspr_pollset_add(fspr_pollset_t *pollset,
                                          const fspr_pollfd_t *descriptor)
{
    fspr_os_sock_t fd;
    pfd_elem_t *elem;
    int res;
    fspr_status_t rv = APR_SUCCESS;

    pollset_lock_rings();

    if (!APR_RING_EMPTY(&(pollset->free_ring), pfd_elem_t, link)) {
        elem = APR_RING_FIRST(&(pollset->free_ring));
        APR_RING_REMOVE(elem, link);
    }
    else {
        elem = (pfd_elem_t *) fspr_palloc(pollset->pool, sizeof(pfd_elem_t));
        APR_RING_ELEM_INIT(elem, link);
    }
    elem->pfd = *descriptor;

    if (descriptor->desc_type == APR_POLL_SOCKET) {
        fd = descriptor->desc.s->socketdes;
    }
    else {
        fd = descriptor->desc.f->filedes;
    }

    res = port_associate(pollset->port_fd, PORT_SOURCE_FD, fd, 
                         get_event(descriptor->reqevents), (void *)elem);

    if (res < 0) {
        rv = APR_ENOMEM;
        APR_RING_INSERT_TAIL(&(pollset->free_ring), elem, pfd_elem_t, link);
    }
    else {
        pollset->nelts++;
        APR_RING_INSERT_TAIL(&(pollset->query_ring), elem, pfd_elem_t, link);
    }

    pollset_unlock_rings();

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_pollset_remove(fspr_pollset_t *pollset,
                                             const fspr_pollfd_t *descriptor)
{
    fspr_os_sock_t fd;
    pfd_elem_t *ep;
    fspr_status_t rv = APR_SUCCESS;
    int res;

    pollset_lock_rings();

    if (descriptor->desc_type == APR_POLL_SOCKET) {
        fd = descriptor->desc.s->socketdes;
    }
    else {
        fd = descriptor->desc.f->filedes;
    }

    res = port_dissociate(pollset->port_fd, PORT_SOURCE_FD, fd);

    if (res < 0) {
        rv = APR_NOTFOUND;
    }

    if (!APR_RING_EMPTY(&(pollset->query_ring), pfd_elem_t, link)) {
        for (ep = APR_RING_FIRST(&(pollset->query_ring));
             ep != APR_RING_SENTINEL(&(pollset->query_ring),
                                     pfd_elem_t, link);
             ep = APR_RING_NEXT(ep, link)) {

            if (descriptor->desc.s == ep->pfd.desc.s) {
                APR_RING_REMOVE(ep, link);
                APR_RING_INSERT_TAIL(&(pollset->dead_ring),
                                     ep, pfd_elem_t, link);
                break;
            }
        }
    }

    if (!APR_RING_EMPTY(&(pollset->add_ring), pfd_elem_t, link)) {
        for (ep = APR_RING_FIRST(&(pollset->add_ring));
             ep != APR_RING_SENTINEL(&(pollset->add_ring),
                                     pfd_elem_t, link);
             ep = APR_RING_NEXT(ep, link)) {

            if (descriptor->desc.s == ep->pfd.desc.s) {
                APR_RING_REMOVE(ep, link);
                APR_RING_INSERT_TAIL(&(pollset->dead_ring),
                                     ep, pfd_elem_t, link);
                break;
            }
        }
    }

    pollset_unlock_rings();

    return rv;
}

APR_DECLARE(fspr_status_t) fspr_pollset_poll(fspr_pollset_t *pollset,
                                           fspr_interval_time_t timeout,
                                           fspr_int32_t *num,
                                           const fspr_pollfd_t **descriptors)
{
    fspr_os_sock_t fd;
    int ret, i;
    unsigned int nget;
    pfd_elem_t *ep;
    struct timespec tv, *tvptr;
    fspr_status_t rv = APR_SUCCESS;

    if (timeout < 0) {
        tvptr = NULL;
    }
    else {
        tv.tv_sec = (long) fspr_time_sec(timeout);
        tv.tv_nsec = (long) fspr_time_msec(timeout);
        tvptr = &tv;
    }

    nget = 1;

    pollset_lock_rings();

    while (!APR_RING_EMPTY(&(pollset->add_ring), pfd_elem_t, link)) {
        ep = APR_RING_FIRST(&(pollset->add_ring));
        APR_RING_REMOVE(ep, link);

        if (ep->pfd.desc_type == APR_POLL_SOCKET) {
            fd = ep->pfd.desc.s->socketdes;
        }
        else {
            fd = ep->pfd.desc.f->filedes;
        }

        port_associate(pollset->port_fd, PORT_SOURCE_FD, 
                           fd, get_event(ep->pfd.reqevents), ep);

        APR_RING_INSERT_TAIL(&(pollset->query_ring), ep, pfd_elem_t, link);

    }

    pollset_unlock_rings();

    ret = port_getn(pollset->port_fd, pollset->port_set, pollset->nalloc,
                    &nget, tvptr);

    (*num) = nget;

    if (ret == -1) {
        (*num) = 0;
        if (errno == ETIME || errno == EINTR) {
            rv = APR_TIMEUP;
        }
        else {
            rv = APR_EGENERAL;
        }
    }
    else if (nget == 0) {
        rv = APR_TIMEUP;
    }
    else {

        pollset_lock_rings();

        for (i = 0; i < nget; i++) {
            pollset->result_set[i] =
                (((pfd_elem_t*)(pollset->port_set[i].portev_user))->pfd);
            pollset->result_set[i].rtnevents =
                get_revent(pollset->port_set[i].portev_events);

            APR_RING_REMOVE((pfd_elem_t*)pollset->port_set[i].portev_user, link);

            APR_RING_INSERT_TAIL(&(pollset->add_ring), 
                                 (pfd_elem_t*)pollset->port_set[i].portev_user,
                                 pfd_elem_t, link);
        }

        pollset_unlock_rings();

        if (descriptors) {
            *descriptors = pollset->result_set;
        }
    }


    pollset_lock_rings();

    /* Shift all PFDs in the Dead Ring to be Free Ring */
    APR_RING_CONCAT(&(pollset->free_ring), &(pollset->dead_ring), pfd_elem_t, link);

    pollset_unlock_rings();

    return rv;
}

#endif /* POLLSET_USES_PORT */
