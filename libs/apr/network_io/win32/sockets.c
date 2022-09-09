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

#include "fspr_arch_networkio.h"
#include "fspr_network_io.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_portable.h"
#include "fspr_strings.h"
#include <string.h>
#include "fspr_arch_inherit.h"
#include "fspr_arch_misc.h"

static char generic_inaddr_any[16] = {0}; /* big enough for IPv4 or IPv6 */

static fspr_status_t socket_cleanup(void *sock)
{
    fspr_socket_t *thesocket = sock;

    if (thesocket->socketdes != INVALID_SOCKET) {
        if (closesocket(thesocket->socketdes) == SOCKET_ERROR) {
            return fspr_get_netos_error();
        }
        thesocket->socketdes = INVALID_SOCKET;
    }
#if APR_HAS_SENDFILE
    if (thesocket->overlapped) {
        CloseHandle(thesocket->overlapped->hEvent);
        thesocket->overlapped = NULL;
    }
#endif
    return APR_SUCCESS;
}

static void set_socket_vars(fspr_socket_t *sock, int family, int type, int protocol)
{
    sock->type = type;
    sock->protocol = protocol;
    fspr_sockaddr_vars_set(sock->local_addr, family, 0);
    fspr_sockaddr_vars_set(sock->remote_addr, family, 0);
}                                                                                                  
static void alloc_socket(fspr_socket_t **new, fspr_pool_t *p)
{
    *new = (fspr_socket_t *)fspr_pcalloc(p, sizeof(fspr_socket_t));
    (*new)->pool = p;
    (*new)->local_addr = (fspr_sockaddr_t *)fspr_pcalloc((*new)->pool,
                                                       sizeof(fspr_sockaddr_t));
    (*new)->local_addr->pool = p;
    
    (*new)->remote_addr = (fspr_sockaddr_t *)fspr_pcalloc((*new)->pool,
                                                        sizeof(fspr_sockaddr_t));
    (*new)->remote_addr->pool = p;
    (*new)->remote_addr_unknown = 1;

    /* Create a pollset with room for one descriptor. */
    /* ### check return codes */
    (void) fspr_pollset_create(&(*new)->pollset, 1, p, 0);
}

APR_DECLARE(fspr_status_t) fspr_socket_protocol_get(fspr_socket_t *sock,
                                                  int *protocol)
{
    *protocol = sock->protocol;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_create(fspr_socket_t **new, int family,
                                            int type, int protocol, 
                                            fspr_pool_t *cont)
{
    int downgrade = (family == AF_UNSPEC);

    if (family == AF_UNSPEC) {
#if APR_HAVE_IPV6
        family = AF_INET6;
#else
        family = AF_INET;
#endif
    }

    alloc_socket(new, cont);

    /* For right now, we are not using socket groups.  We may later.
     * No flags to use when creating a socket, so use 0 for that parameter as well.
     */
    (*new)->socketdes = socket(family, type, protocol);
#if APR_HAVE_IPV6
    if ((*new)->socketdes == INVALID_SOCKET && downgrade) {
        family = AF_INET;
        (*new)->socketdes = socket(family, type, protocol);
    }
#endif

    if ((*new)->socketdes == INVALID_SOCKET) {
        return fspr_get_netos_error();
    }

#ifdef WIN32
    /* Socket handles are never truly inheritable, there are too many
     * bugs associated.  WSADuplicateSocket will copy them, but for our
     * purposes, always transform the socket() created as a non-inherited
     * handle
     */
#if APR_HAS_UNICODE_FS
    IF_WIN_OS_IS_UNICODE {
        /* A different approach.  Many users report errors such as 
         * (32538)An operation was attempted on something that is not 
         * a socket.  : Parent: WSADuplicateSocket failed...
         *
         * This appears that the duplicated handle is no longer recognized
         * as a socket handle.  SetHandleInformation should overcome that
         * problem by not altering the handle identifier.  But this won't
         * work on 9x - it's unsupported.
         */
        SetHandleInformation((HANDLE) (*new)->socketdes, 
                             HANDLE_FLAG_INHERIT, 0);
    }
#endif
#if APR_HAS_ANSI_FS
    ELSE_WIN_OS_IS_ANSI {
        HANDLE hProcess = GetCurrentProcess();
        HANDLE dup;
        if (DuplicateHandle(hProcess, (HANDLE) (*new)->socketdes, hProcess, 
                            &dup, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            closesocket((*new)->socketdes);
            (*new)->socketdes = (SOCKET) dup;
        }
    }
#endif

#endif /* def WIN32 */

    set_socket_vars(*new, family, type, protocol);

    (*new)->timeout = -1;
    (*new)->disconnected = 0;

    fspr_pool_cleanup_register((*new)->pool, (void *)(*new), 
                        socket_cleanup, fspr_pool_cleanup_null);

    return APR_SUCCESS;
} 

APR_DECLARE(fspr_status_t) fspr_socket_shutdown(fspr_socket_t *thesocket,
                                              fspr_shutdown_how_e how)
{
    int winhow = 0;

#ifdef SD_RECEIVE
    switch (how) {
        case APR_SHUTDOWN_READ: {
            winhow = SD_RECEIVE;
            break;
        }
        case APR_SHUTDOWN_WRITE: {
            winhow = SD_SEND;
            break;
        }
        case APR_SHUTDOWN_READWRITE: {
            winhow = SD_BOTH;
            break;
        }
        default:
            return APR_BADARG;
    }
#endif
    if (shutdown(thesocket->socketdes, winhow) == 0) {
        return APR_SUCCESS;
    }
    else {
        return fspr_get_netos_error();
    }
}

APR_DECLARE(fspr_status_t) fspr_socket_close(fspr_socket_t *thesocket)
{
    fspr_pool_cleanup_kill(thesocket->pool, thesocket, socket_cleanup);
    return socket_cleanup(thesocket);
}

APR_DECLARE(fspr_status_t) fspr_socket_bind(fspr_socket_t *sock,
                                          fspr_sockaddr_t *sa)
{
    if (bind(sock->socketdes, 
             (struct sockaddr *)&sa->sa, 
             sa->salen) == -1) {
        return fspr_get_netos_error();
    }
    else {
        sock->local_addr = sa;
        if (sock->local_addr->sa.sin.sin_port == 0) {
            sock->local_port_unknown = 1; /* ephemeral port */
        }
        return APR_SUCCESS;
    }
}

APR_DECLARE(fspr_status_t) fspr_socket_listen(fspr_socket_t *sock,
                                            fspr_int32_t backlog)
{
    if (listen(sock->socketdes, backlog) == SOCKET_ERROR)
        return fspr_get_netos_error();
    else
        return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_accept(fspr_socket_t **new, 
                                            fspr_socket_t *sock, fspr_pool_t *p)
{
    SOCKET s;
#if APR_HAVE_IPV6
    struct sockaddr_storage sa;
#else
    struct sockaddr sa;
#endif
    int salen = sizeof(sock->remote_addr->sa);

    /* Don't allocate the memory until after we call accept. This allows
       us to work with nonblocking sockets. */
    s = accept(sock->socketdes, (struct sockaddr *)&sa, &salen);
    if (s == INVALID_SOCKET) {
        return fspr_get_netos_error();
    }

    alloc_socket(new, p);
    set_socket_vars(*new, sock->local_addr->sa.sin.sin_family, SOCK_STREAM, 
                    sock->protocol);

    (*new)->timeout = -1;   
    (*new)->disconnected = 0;

    (*new)->socketdes = s;
    /* XXX next line looks bogus w.r.t. AF_INET6 support */
    (*new)->remote_addr->salen = sizeof((*new)->remote_addr->sa);
    memcpy (&(*new)->remote_addr->sa, &sa, salen);
    *(*new)->local_addr = *sock->local_addr;

    /* The above assignment just overwrote the pool entry. Setting the local_addr 
       pool for the accepted socket back to what it should be.  Otherwise all 
       allocations for this socket will come from a server pool that is not
       freed until the process goes down.*/
    (*new)->local_addr->pool = p;

    /* fix up any pointers which are no longer valid */
    if (sock->local_addr->sa.sin.sin_family == AF_INET) {
        (*new)->local_addr->ipaddr_ptr = &(*new)->local_addr->sa.sin.sin_addr;
    }
#if APR_HAVE_IPV6
    else if (sock->local_addr->sa.sin.sin_family == AF_INET6) {
        (*new)->local_addr->ipaddr_ptr = &(*new)->local_addr->sa.sin6.sin6_addr;
    }
#endif
    (*new)->remote_addr->port = ntohs((*new)->remote_addr->sa.sin.sin_port);
    if (sock->local_port_unknown) {
        /* not likely for a listening socket, but theoretically possible :) */
        (*new)->local_port_unknown = 1;
    }

#if APR_TCP_NODELAY_INHERITED
    if (fspr_is_option_set(sock, APR_TCP_NODELAY) == 1) {
        fspr_set_option(*new, APR_TCP_NODELAY, 1);
    }
#endif /* TCP_NODELAY_INHERITED */
#if APR_O_NONBLOCK_INHERITED
    if (fspr_is_option_set(sock, APR_SO_NONBLOCK) == 1) {
        fspr_set_option(*new, APR_SO_NONBLOCK, 1);
    }
#endif /* APR_O_NONBLOCK_INHERITED */

    if (sock->local_interface_unknown ||
        !memcmp(sock->local_addr->ipaddr_ptr,
                generic_inaddr_any,
                sock->local_addr->ipaddr_len)) {
        /* If the interface address inside the listening socket's local_addr wasn't
         * up-to-date, we don't know local interface of the connected socket either.
         *
         * If the listening socket was not bound to a specific interface, we
         * don't know the local_addr of the connected socket.
         */
        (*new)->local_interface_unknown = 1;
    }

    fspr_pool_cleanup_register((*new)->pool, (void *)(*new), 
                        socket_cleanup, fspr_pool_cleanup_null);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_connect(fspr_socket_t *sock, 
                                             fspr_sockaddr_t *sa)
{
    fspr_status_t rv;

    if ((sock->socketdes == INVALID_SOCKET) || (!sock->local_addr)) {
        return APR_ENOTSOCK;
    }

    if (connect(sock->socketdes, (const struct sockaddr *)&sa->sa.sin,
                sa->salen) == SOCKET_ERROR) {
        int rc;
        struct timeval tv, *tvptr;
        fd_set wfdset, efdset;

        rv = fspr_get_netos_error();
        if (rv != APR_FROM_OS_ERROR(WSAEWOULDBLOCK)) {
            return rv;
        }

        if (sock->timeout == 0) {
            /* Tell the app that the connect is in progress...
             * Gotta play some games here.  connect on Unix will return 
             * EINPROGRESS under the same circumstances that Windows 
             * returns WSAEWOULDBLOCK. Do some adhoc canonicalization...
             */
            return APR_FROM_OS_ERROR(WSAEINPROGRESS);
        }

        /* wait for the connect to complete or timeout */
        FD_ZERO(&wfdset);
        FD_SET(sock->socketdes, &wfdset);
        FD_ZERO(&efdset);
        FD_SET(sock->socketdes, &efdset);

        if (sock->timeout < 0) {
            tvptr = NULL;
        }
        else {
            /* casts for winsock/timeval definition */
            tv.tv_sec =  (long)fspr_time_sec(sock->timeout);
            tv.tv_usec = (int)fspr_time_usec(sock->timeout);
            tvptr = &tv;
        }
        rc = select(FD_SETSIZE+1, NULL, &wfdset, &efdset, tvptr);
        if (rc == SOCKET_ERROR) {
            return fspr_get_netos_error();
        }
        else if (!rc) {
            return APR_FROM_OS_ERROR(WSAETIMEDOUT);
        }
        /* Evaluate the efdset */
        if (FD_ISSET(sock->socketdes, &efdset)) {
            /* The connect failed. */
            int rclen = sizeof(rc);
            if (getsockopt(sock->socketdes, SOL_SOCKET, SO_ERROR, (char*) &rc, &rclen)) {
                return fspr_get_netos_error();
            }
            return APR_FROM_OS_ERROR(rc);
        }
    }
    /* connect was OK .. amazing */
    sock->remote_addr = sa;
    if (sock->local_addr->sa.sin.sin_port == 0) {
        sock->local_port_unknown = 1;
    }
    if (!memcmp(sock->local_addr->ipaddr_ptr,
                generic_inaddr_any,
                sock->local_addr->ipaddr_len)) {
        /* not bound to specific local interface; connect() had to assign
         * one for the socket
         */
        sock->local_interface_unknown = 1;
    }
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_type_get(fspr_socket_t *sock, int *type)
{
    *type = sock->type;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_data_get(void **data, const char *key,
                                             fspr_socket_t *sock)
{
    sock_userdata_t *cur = sock->userdata;

    *data = NULL;

    while (cur) {
        if (!strcmp(cur->key, key)) {
            *data = cur->data;
            break;
        }
        cur = cur->next;
    }

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_data_set(fspr_socket_t *sock, void *data,
                                             const char *key,
                                             fspr_status_t (*cleanup)(void *))
{
    sock_userdata_t *new = fspr_palloc(sock->pool, sizeof(sock_userdata_t));

    new->key = fspr_pstrdup(sock->pool, key);
    new->data = data;
    new->next = sock->userdata;
    sock->userdata = new;

    if (cleanup) {
        fspr_pool_cleanup_register(sock->pool, data, cleanup, cleanup);
    }

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_sock_get(fspr_os_sock_t *thesock,
                                          fspr_socket_t *sock)
{
    *thesock = sock->socketdes;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_sock_make(fspr_socket_t **fspr_sock,
                                           fspr_os_sock_info_t *os_sock_info,
                                           fspr_pool_t *cont)
{
    alloc_socket(fspr_sock, cont);
    set_socket_vars(*fspr_sock, os_sock_info->family, os_sock_info->type, os_sock_info->protocol);
    (*fspr_sock)->timeout = -1;
    (*fspr_sock)->disconnected = 0;
    (*fspr_sock)->socketdes = *os_sock_info->os_sock;
    if (os_sock_info->local) {
        memcpy(&(*fspr_sock)->local_addr->sa.sin, 
               os_sock_info->local, 
               (*fspr_sock)->local_addr->salen);
        (*fspr_sock)->local_addr->pool = cont;
        /* XXX IPv6 - this assumes sin_port and sin6_port at same offset */
        (*fspr_sock)->local_addr->port = ntohs((*fspr_sock)->local_addr->sa.sin.sin_port);
    }
    else {
        (*fspr_sock)->local_port_unknown = (*fspr_sock)->local_interface_unknown = 1;
    }
    if (os_sock_info->remote) {
        memcpy(&(*fspr_sock)->remote_addr->sa.sin, 
               os_sock_info->remote,
               (*fspr_sock)->remote_addr->salen);
        (*fspr_sock)->remote_addr->pool = cont;
        /* XXX IPv6 - this assumes sin_port and sin6_port at same offset */
        (*fspr_sock)->remote_addr->port = ntohs((*fspr_sock)->remote_addr->sa.sin.sin_port);
    }
    else {
        (*fspr_sock)->remote_addr_unknown = 1;
    }
        
    fspr_pool_cleanup_register((*fspr_sock)->pool, (void *)(*fspr_sock), 
                        socket_cleanup, fspr_pool_cleanup_null);

    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_os_sock_put(fspr_socket_t **sock,
                                          fspr_os_sock_t *thesock,
                                          fspr_pool_t *cont)
{
    if ((*sock) == NULL) {
        alloc_socket(sock, cont);
        /* XXX figure out the actual socket type here */
        /* *or* just decide that fspr_os_sock_put() has to be told the family and type */
        set_socket_vars(*sock, AF_INET, SOCK_STREAM, 0);
        (*sock)->timeout = -1;
        (*sock)->disconnected = 0;
    }
    (*sock)->local_port_unknown = (*sock)->local_interface_unknown = 1;
    (*sock)->remote_addr_unknown = 1;
    (*sock)->socketdes = *thesock;
    return APR_SUCCESS;
}


/* Sockets cannot be inherited through the standard sockets
 * inheritence.  WSADuplicateSocket must be used.
 * This is not trivial to implement.
 */

APR_DECLARE(fspr_status_t) fspr_socket_inherit_set(fspr_socket_t *socket)    
{    
    return APR_ENOTIMPL;
}    

APR_DECLARE(fspr_status_t) fspr_socket_inherit_unset(fspr_socket_t *socket)    
{    
    return APR_ENOTIMPL;
}    

APR_POOL_IMPLEMENT_ACCESSOR(socket);
