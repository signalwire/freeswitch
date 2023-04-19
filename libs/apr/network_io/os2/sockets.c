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
#include "fspr_arch_inherit.h"
#include "fspr_network_io.h"
#include "fspr_general.h"
#include "fspr_portable.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "fspr_arch_os2calls.h"

static fspr_status_t socket_cleanup(void *sock)
{
    fspr_socket_t *thesocket = sock;

    if (thesocket->socketdes < 0) {
        return APR_EINVALSOCK;
    }

    if (soclose(thesocket->socketdes) == 0) {
        thesocket->socketdes = -1;
        return APR_SUCCESS;
    }
    else {
        return APR_OS2_STATUS(sock_errno());
    }
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

APR_DECLARE(fspr_status_t) fspr_socket_protocol_get(fspr_socket_t *sock, int *protocol)
{
    *protocol = sock->protocol;
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_create(fspr_socket_t **new, int family, int type,
                                            int protocol, fspr_pool_t *cont)
{
    int downgrade = (family == AF_UNSPEC);
    fspr_pollfd_t pfd;

    if (family == AF_UNSPEC) {
#if APR_HAVE_IPV6
        family = AF_INET6;
#else
        family = AF_INET;
#endif
    }

    alloc_socket(new, cont);

    (*new)->socketdes = socket(family, type, protocol);
#if APR_HAVE_IPV6
    if ((*new)->socketdes < 0 && downgrade) {
        family = AF_INET;
        (*new)->socketdes = socket(family, type, protocol);
    }
#endif

    if ((*new)->socketdes < 0) {
        return APR_OS2_STATUS(sock_errno());
    }
    set_socket_vars(*new, family, type, protocol);

    (*new)->timeout = -1;
    (*new)->nonblock = FALSE;
    fspr_pool_cleanup_register((*new)->pool, (void *)(*new), 
                        socket_cleanup, fspr_pool_cleanup_null);

    return APR_SUCCESS;
} 

APR_DECLARE(fspr_status_t) fspr_socket_shutdown(fspr_socket_t *thesocket, 
                                              fspr_shutdown_how_e how)
{
    if (shutdown(thesocket->socketdes, how) == 0) {
        return APR_SUCCESS;
    }
    else {
        return APR_OS2_STATUS(sock_errno());
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
             sa->salen) == -1)
        return APR_OS2_STATUS(sock_errno());
    else {
        sock->local_addr = sa;
        /* XXX IPv6 - this assumes sin_port and sin6_port at same offset */
        if (sock->local_addr->sa.sin.sin_port == 0) { /* no need for ntohs() when comparing w/ 0 */
            sock->local_port_unknown = 1; /* kernel got us an ephemeral port */
        }
        return APR_SUCCESS;
    }
}

APR_DECLARE(fspr_status_t) fspr_socket_listen(fspr_socket_t *sock, 
                                            fspr_int32_t backlog)
{
    if (listen(sock->socketdes, backlog) == -1)
        return APR_OS2_STATUS(sock_errno());
    else
        return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_accept(fspr_socket_t **new, 
                                            fspr_socket_t *sock,
                                            fspr_pool_t *connection_context)
{
    alloc_socket(new, connection_context);
    set_socket_vars(*new, sock->local_addr->sa.sin.sin_family, SOCK_STREAM, sock->protocol);

    (*new)->timeout = -1;
    (*new)->nonblock = FALSE;

    (*new)->socketdes = accept(sock->socketdes, 
                               (struct sockaddr *)&(*new)->remote_addr->sa,
                               &(*new)->remote_addr->salen);

    if ((*new)->socketdes < 0) {
        return APR_OS2_STATUS(sock_errno());
    }

    *(*new)->local_addr = *sock->local_addr;
    (*new)->local_addr->pool = connection_context;
    (*new)->remote_addr->port = ntohs((*new)->remote_addr->sa.sin.sin_port);

    /* fix up any pointers which are no longer valid */
    if (sock->local_addr->sa.sin.sin_family == AF_INET) {
        (*new)->local_addr->ipaddr_ptr = &(*new)->local_addr->sa.sin.sin_addr;
    }

    fspr_pool_cleanup_register((*new)->pool, (void *)(*new), 
                        socket_cleanup, fspr_pool_cleanup_null);
    return APR_SUCCESS;
}

APR_DECLARE(fspr_status_t) fspr_socket_connect(fspr_socket_t *sock,
                                             fspr_sockaddr_t *sa)
{
    if ((connect(sock->socketdes, (struct sockaddr *)&sa->sa.sin, 
                 sa->salen) < 0) &&
        (sock_errno() != SOCEINPROGRESS)) {
        return APR_OS2_STATUS(sock_errno());
    }
    else {
        int namelen = sizeof(sock->local_addr->sa.sin);
        getsockname(sock->socketdes, (struct sockaddr *)&sock->local_addr->sa.sin, 
                    &namelen);
        sock->remote_addr = sa;
        return APR_SUCCESS;
    }
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



APR_DECLARE(fspr_status_t) fspr_socket_data_set(fspr_socket_t *sock, void *data, const char *key,
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

APR_DECLARE(fspr_status_t) fspr_os_sock_get(fspr_os_sock_t *thesock, fspr_socket_t *sock)
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
        memcpy(&(*fspr_sock)->remote_addr->sa.sin, 
               os_sock_info->remote,
               (*fspr_sock)->remote_addr->salen);
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

APR_DECLARE(fspr_status_t) fspr_os_sock_put(fspr_socket_t **sock, fspr_os_sock_t *thesock, fspr_pool_t *cont)
{
    if (cont == NULL) {
        return APR_ENOPOOL;
    }
    if ((*sock) == NULL) {
        alloc_socket(sock, cont);
        set_socket_vars(*sock, AF_INET, SOCK_STREAM, 0);
        (*sock)->timeout = -1;
    }

    (*sock)->local_port_unknown = (*sock)->local_interface_unknown = 1;
    (*sock)->remote_addr_unknown = 1;
    (*sock)->socketdes = *thesock;
    return APR_SUCCESS;
}

APR_POOL_IMPLEMENT_ACCESSOR(socket);

APR_IMPLEMENT_INHERIT_SET(socket, inherit, pool, socket_cleanup)

APR_IMPLEMENT_INHERIT_UNSET(socket, inherit, pool, socket_cleanup)

