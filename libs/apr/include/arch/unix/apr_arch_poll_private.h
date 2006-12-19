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

#ifndef APR_ARCH_POLL_PRIVATE_H
#define APR_ARCH_POLL_PRIVATE_H

#include "apr.h"
#include "apr_poll.h"
#include "apr_time.h"
#include "apr_portable.h"
#include "apr_arch_networkio.h"
#include "apr_arch_file_io.h"

#if HAVE_POLL_H
#include <poll.h>
#endif

#if HAVE_SYS_POLL_H
#include <sys/poll.h>
#endif

#ifdef HAVE_PORT_CREATE
#include <port.h>
#include <sys/port_impl.h>
#endif

#ifdef HAVE_KQUEUE
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>
#endif

#ifdef HAVE_EPOLL
#include <sys/epoll.h>
#endif

#ifdef NETWARE
#define HAS_SOCKETS(dt) (dt == APR_POLL_SOCKET) ? 1 : 0
#define HAS_PIPES(dt) (dt == APR_POLL_FILE) ? 1 : 0
#endif

/* Choose the best method platform specific to use in apr_pollset */
#ifdef HAVE_KQUEUE
#define POLLSET_USES_KQUEUE
#elif defined(HAVE_PORT_CREATE)
#define POLLSET_USES_PORT
#elif defined(HAVE_EPOLL)
#define POLLSET_USES_EPOLL
#elif defined(HAVE_POLL)
#define POLLSET_USES_POLL
#else
#define POLLSET_USES_SELECT
#endif

#ifdef HAVE_POLL
#define POLL_USES_POLL
#else
#define POLL_USES_SELECT
#endif

#if defined(POLLSET_USES_KQUEUE) || defined(POLLSET_USES_EPOLL) || defined(POLLSET_USES_PORT)

#include "apr_ring.h"

#if APR_HAS_THREADS
#include "apr_thread_mutex.h"
#define pollset_lock_rings() \
    if (pollset->flags & APR_POLLSET_THREADSAFE) \
        apr_thread_mutex_lock(pollset->ring_lock);
#define pollset_unlock_rings() \
    if (pollset->flags & APR_POLLSET_THREADSAFE) \
        apr_thread_mutex_unlock(pollset->ring_lock);
#else
#define pollset_lock_rings()
#define pollset_unlock_rings()
#endif

typedef struct pfd_elem_t pfd_elem_t;

struct pfd_elem_t {
    APR_RING_ENTRY(pfd_elem_t) link;
    apr_pollfd_t pfd;
};

#endif

#endif /* APR_ARCH_POLL_PRIVATE_H */
