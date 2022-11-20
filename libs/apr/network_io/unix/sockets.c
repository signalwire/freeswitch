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
#include "fspr_strings.h"
#include "fspr_support.h"
#include "fspr_portable.h"
#include "fspr_arch_inherit.h"

#ifdef BEOS_R5
#undef close
#define close closesocket
#endif /* BEOS_R5 */

static char generic_inaddr_any[16] = {0}; /* big enough for IPv4 or IPv6 */

static fspr_status_t socket_cleanup(void *sock)
{
    fspr_socket_t *thesocket = sock;

	if (!thesocket) {
		return APR_ENOTSOCK;
	}

	if (thesocket->socketdes == -1) {
		return APR_SUCCESS;
	}

    if (close(thesocket->socketdes) == 0) {
        thesocket->socketdes = -1;
        return APR_SUCCESS;
    }
    else {
        return errno;
    }
}

static void set_socket_vars(fspr_socket_t *sock, int family, int type, int protocol)
{
    sock->type = type;
    sock->protocol = protocol;
    fspr_sockaddr_vars_set(sock->local_addr, family, 0);
    fspr_sockaddr_vars_set(sock->remote_addr, family, 0);
    sock->options = 0;
#if defined(BEOS) && !defined(BEOS_BONE)
    /* BeOS pre-BONE has TCP_NODELAY on by default and it can't be
     * switched off!
     */
    sock->options |= APR_TCP_NODELAY;
#endif
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
#ifndef WAITIO_USES_POLL
    /* Create a pollset with room for one descriptor. */
    /* ### check return codes */
    (void) fspr_pollset_create(&(*new)->pollset, 1, p, 0);
#endif
}

fspr_status_t fspr_socket_protocol_get(fspr_socket_t *sock, int *protocol)
{
    *protocol = sock->protocol;
    return APR_SUCCESS;
}

fspr_status_t fspr_socket_create(fspr_socket_t **new, int ofamily, int type,
                               int protocol, fspr_pool_t *cont)
{
    int family = ofamily;

    if (family == APR_UNSPEC) {
#if APR_HAVE_IPV6
        family = APR_INET6;
#else
        family = APR_INET;
#endif
    }

    alloc_socket(new, cont);

#ifndef BEOS_R5
    (*new)->socketdes = socket(family, type, protocol);
#else
    /* For some reason BeOS R5 has an unconventional protocol numbering,
     * so we need to translate here. */
    switch (protocol) {
    case 0:
        (*new)->socketdes = socket(family, type, 0);
        break;
    case APR_PROTO_TCP:
        (*new)->socketdes = socket(family, type, IPPROTO_TCP);
        break;
    case APR_PROTO_UDP:
        (*new)->socketdes = socket(family, type, IPPROTO_UDP);
        break;
    case APR_PROTO_SCTP:
    default:
        errno = EPROTONOSUPPORT;
        (*new)->socketdes = -1;
        break;
    }
#endif /* BEOS_R5 */

#if APR_HAVE_IPV6
    if ((*new)->socketdes < 0 && ofamily == APR_UNSPEC) {
        family = APR_INET;
        (*new)->socketdes = socket(family, type, protocol);
    }
#endif

    if ((*new)->socketdes < 0) {
        return errno;
    }
    set_socket_vars(*new, family, type, protocol);

    (*new)->timeout = -1;
    (*new)->inherit = 0;
    fspr_pool_cleanup_register((*new)->pool, (void *)(*new), socket_cleanup,
                              socket_cleanup);

    return APR_SUCCESS;
} 

fspr_status_t fspr_socket_shutdown(fspr_socket_t *thesocket, 
                                 fspr_shutdown_how_e how)
{
    return (shutdown(thesocket->socketdes, how) == -1) ? errno : APR_SUCCESS;
}

fspr_status_t fspr_socket_close(fspr_socket_t *thesocket)
{
    return fspr_pool_cleanup_run(thesocket->pool, thesocket, socket_cleanup);
}

fspr_status_t fspr_socket_bind(fspr_socket_t *sock, fspr_sockaddr_t *sa)
{
    if (bind(sock->socketdes, 
             (struct sockaddr *)&sa->sa, sa->salen) == -1) {
        return errno;
    }
    else {
        sock->local_addr = sa;
        /* XXX IPv6 - this assumes sin_port and sin6_port at same offset */
        if (sock->local_addr->sa.sin.sin_port == 0) { /* no need for ntohs() when comparing w/ 0 */
            sock->local_port_unknown = 1; /* kernel got us an ephemeral port */
        }
        return APR_SUCCESS;
    }
}

fspr_status_t fspr_socket_listen(fspr_socket_t *sock, fspr_int32_t backlog)
{
    if (listen(sock->socketdes, backlog) == -1)
        return errno;
    else
        return APR_SUCCESS;
}

fspr_status_t fspr_socket_accept(fspr_socket_t **new, fspr_socket_t *sock,
                               fspr_pool_t *connection_context)
{
    alloc_socket(new, connection_context);
    set_socket_vars(*new, sock->local_addr->sa.sin.sin_family, SOCK_STREAM, sock->protocol);

#ifndef HAVE_POLL
    (*new)->connected = 1;
#endif
    (*new)->timeout = -1;
    
    (*new)->socketdes = accept(sock->socketdes, 
                               (struct sockaddr *)&(*new)->remote_addr->sa,
                               &(*new)->remote_addr->salen);

    if ((*new)->socketdes < 0) {
        return errno;
    }
#ifdef TPF
    if ((*new)->socketdes == 0) { 
        /* 0 is an invalid socket for TPF */
        return APR_EINTR;
    }
#endif

    (*new)->remote_addr_unknown = 0;

    *(*new)->local_addr = *sock->local_addr;

    /* The above assignment just overwrote the pool entry. Setting the local_addr 
       pool for the accepted socket back to what it should be.  Otherwise all 
       allocations for this socket will come from a server pool that is not
       freed until the process goes down.*/
    (*new)->local_addr->pool = connection_context;

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

    (*new)->inherit = 0;
    fspr_pool_cleanup_register((*new)->pool, (void *)(*new), socket_cleanup,
                              socket_cleanup);
    return APR_SUCCESS;
}

fspr_status_t fspr_socket_connect(fspr_socket_t *sock, fspr_sockaddr_t *sa)
{
    int rc;        

    do {
        rc = connect(sock->socketdes,
                     (const struct sockaddr *)&sa->sa.sin,
                     sa->salen);
    } while (rc == -1 && errno == EINTR);

    /* we can see EINPROGRESS the first time connect is called on a non-blocking
     * socket; if called again, we can see EALREADY
     */
    if ((rc == -1) && (errno == EINPROGRESS || errno == EALREADY)
                   && (sock->timeout > 0)) {
        rc = fspr_wait_for_io_or_timeout(NULL, sock, 0);
        if (rc != APR_SUCCESS) {
            return rc;
        }

#ifdef SO_ERROR
        {
            int error;
            fspr_socklen_t len = sizeof(error);
            if ((rc = getsockopt(sock->socketdes, SOL_SOCKET, SO_ERROR, 
                                 (char *)&error, &len)) < 0) {
                return errno;
            }
            if (error) {
                return error;
            }
        }
#endif /* SO_ERROR */
    }

    if (rc == -1 && errno != EISCONN) {
        return errno;
    }

    if (memcmp(sa->ipaddr_ptr, generic_inaddr_any, sa->ipaddr_len)) {
        /* A real remote address was passed in.  If the unspecified
         * address was used, the actual remote addr will have to be
         * determined using getpeername() if required. */
        /* ### this should probably be a structure copy + fixup as per
         * _accept()'s handling of local_addr */
        sock->remote_addr = sa;
        sock->remote_addr_unknown = 0;
    }

    if (sock->local_addr->port == 0) {
        /* connect() got us an ephemeral port */
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
#ifndef HAVE_POLL
    sock->connected=1;
#endif
    return APR_SUCCESS;
}

fspr_status_t fspr_socket_type_get(fspr_socket_t *sock, int *type)
{
    *type = sock->type;
    return APR_SUCCESS;
}

fspr_status_t fspr_socket_data_get(void **data, const char *key, fspr_socket_t *sock)
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

fspr_status_t fspr_socket_data_set(fspr_socket_t *sock, void *data, const char *key,
                                 fspr_status_t (*cleanup) (void *))
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

fspr_status_t fspr_os_sock_get(fspr_os_sock_t *thesock, fspr_socket_t *sock)
{
    *thesock = sock->socketdes;
    return APR_SUCCESS;
}

fspr_status_t fspr_os_sock_make(fspr_socket_t **fspr_sock, 
                              fspr_os_sock_info_t *os_sock_info, 
                              fspr_pool_t *cont)
{
    alloc_socket(fspr_sock, cont);
    set_socket_vars(*fspr_sock, os_sock_info->family, os_sock_info->type, os_sock_info->protocol);
    (*fspr_sock)->timeout = -1;
    (*fspr_sock)->socketdes = *os_sock_info->os_sock;
    if (os_sock_info->local) {
        memcpy(&(*fspr_sock)->local_addr->sa.sin, 
               os_sock_info->local, 
               (*fspr_sock)->local_addr->salen);
        /* XXX IPv6 - this assumes sin_port and sin6_port at same offset */
        (*fspr_sock)->local_addr->port = ntohs((*fspr_sock)->local_addr->sa.sin.sin_port);
    }
    else {
        (*fspr_sock)->local_port_unknown = (*fspr_sock)->local_interface_unknown = 1;
    }
    if (os_sock_info->remote) {
#ifndef HAVE_POLL
        (*fspr_sock)->connected = 1;
#endif
        memcpy(&(*fspr_sock)->remote_addr->sa.sin, 
               os_sock_info->remote,
               (*fspr_sock)->remote_addr->salen);
        /* XXX IPv6 - this assumes sin_port and sin6_port at same offset */
        (*fspr_sock)->remote_addr->port = ntohs((*fspr_sock)->remote_addr->sa.sin.sin_port);
    }
    else {
        (*fspr_sock)->remote_addr_unknown = 1;
    }
        
    (*fspr_sock)->inherit = 0;
    fspr_pool_cleanup_register((*fspr_sock)->pool, (void *)(*fspr_sock), 
                              socket_cleanup, socket_cleanup);
    return APR_SUCCESS;
}

fspr_status_t fspr_os_sock_put(fspr_socket_t **sock, fspr_os_sock_t *thesock, 
                           fspr_pool_t *cont)
{
    /* XXX Bogus assumption that *sock points at anything legit */
    if ((*sock) == NULL) {
        alloc_socket(sock, cont);
        /* XXX IPv6 figure out the family here! */
        /* XXX figure out the actual socket type here */
        /* *or* just decide that fspr_os_sock_put() has to be told the family and type */
        set_socket_vars(*sock, APR_INET, SOCK_STREAM, 0);
        (*sock)->timeout = -1;
    }
    (*sock)->local_port_unknown = (*sock)->local_interface_unknown = 1;
    (*sock)->remote_addr_unknown = 1;
    (*sock)->socketdes = *thesock;
    return APR_SUCCESS;
}

APR_POOL_IMPLEMENT_ACCESSOR(socket)

APR_IMPLEMENT_INHERIT_SET(socket, inherit, pool, socket_cleanup)

APR_IMPLEMENT_INHERIT_UNSET(socket, inherit, pool, socket_cleanup)
