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
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_network_io.h"
#include "fspr_support.h"
#include "fspr_lib.h"
#include <sys/time.h>


APR_DECLARE(fspr_status_t) fspr_socket_sendto(fspr_socket_t *sock, 
                                            fspr_sockaddr_t *where,
                                            fspr_int32_t flags, const char *buf,
                                            fspr_size_t *len)
{
    fspr_ssize_t rv;
    int serrno;

    do {
        rv = sendto(sock->socketdes, buf, (*len), flags, 
                    (struct sockaddr*)&where->sa,
                    where->salen);
    } while (rv == -1 && (serrno = sock_errno()) == EINTR);

    if (rv == -1 && serrno == SOCEWOULDBLOCK && sock->timeout != 0) {
        fspr_status_t arv = fspr_wait_for_io_or_timeout(NULL, sock, 0);

        if (arv != APR_SUCCESS) {
            *len = 0;
            return arv;
        } else {
            do {
                rv = sendto(sock->socketdes, buf, *len, flags,
                            (const struct sockaddr*)&where->sa,
                            where->salen);
            } while (rv == -1 && (serrno = sock_errno()) == SOCEINTR);
        }
    }

    if (rv == -1) {
        *len = 0;
        return APR_FROM_OS_ERROR(serrno);
    }

    *len = rv;
    return APR_SUCCESS;
}



APR_DECLARE(fspr_status_t) fspr_socket_recvfrom(fspr_sockaddr_t *from,
                                              fspr_socket_t *sock,
                                              fspr_int32_t flags, char *buf,
                                              fspr_size_t *len)
{
    fspr_ssize_t rv;
    int serrno;

    do {
        rv = recvfrom(sock->socketdes, buf, (*len), flags, 
                      (struct sockaddr*)&from->sa, &from->salen);
    } while (rv == -1 && (serrno = sock_errno()) == EINTR);

    if (rv == -1 && serrno == SOCEWOULDBLOCK && sock->timeout != 0) {
        fspr_status_t arv = fspr_wait_for_io_or_timeout(NULL, sock, 1);

        if (arv != APR_SUCCESS) {
            *len = 0;
            return arv;
        } else {
            do {
                rv = recvfrom(sock->socketdes, buf, *len, flags,
                              (struct sockaddr*)&from->sa, &from->salen);
                } while (rv == -1 && (serrno = sock_errno()) == EINTR);
        }
    }

    if (rv == -1) {
        (*len) = 0;
        return APR_FROM_OS_ERROR(serrno);
    }

    (*len) = rv;

    if (rv == 0 && sock->type == SOCK_STREAM)
        return APR_EOF;

    return APR_SUCCESS;
}
