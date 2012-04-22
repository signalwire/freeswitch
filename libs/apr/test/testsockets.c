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

#include "apr_network_io.h"
#include "apr_errno.h"
#include "apr_general.h"
#include "apr_lib.h"
#include "testutil.h"

#define STRLEN 21

static void tcp_socket(abts_case *tc, void *data)
{
    apr_status_t rv;
    apr_socket_t *sock = NULL;
    int type;

    rv = apr_socket_create(&sock, APR_INET, SOCK_STREAM, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, sock);

    rv = apr_socket_type_get(sock, &type);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, SOCK_STREAM, type);

    apr_socket_close(sock);
}

static void udp_socket(abts_case *tc, void *data)
{
    apr_status_t rv;
    apr_socket_t *sock = NULL;
    int type;

    rv = apr_socket_create(&sock, APR_INET, SOCK_DGRAM, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, sock);

    rv = apr_socket_type_get(sock, &type);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, SOCK_DGRAM, type);

    apr_socket_close(sock);
}

/* On recent Linux systems, whilst IPv6 is always supported by glibc,
 * socket(AF_INET6, ...) calls will fail with EAFNOSUPPORT if the
 * "ipv6" kernel module is not loaded.  */
#ifdef EAFNOSUPPORT
#define V6_NOT_ENABLED(e) ((e) == EAFNOSUPPORT)
#else
#define V6_NOT_ENABLED(e) (0)
#endif

static void tcp6_socket(abts_case *tc, void *data)
{
#if APR_HAVE_IPV6
    apr_status_t rv;
    apr_socket_t *sock = NULL;

    rv = apr_socket_create(&sock, APR_INET6, SOCK_STREAM, 0, p);
    if (V6_NOT_ENABLED(rv)) {
        ABTS_NOT_IMPL(tc, "IPv6 not enabled");
        return;
    }
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, sock);
    apr_socket_close(sock);
#else
    ABTS_NOT_IMPL(tc, "IPv6");
#endif
}

static void udp6_socket(abts_case *tc, void *data)
{
#if APR_HAVE_IPV6
    apr_status_t rv;
    apr_socket_t *sock = NULL;

    rv = apr_socket_create(&sock, APR_INET6, SOCK_DGRAM, 0, p);
    if (V6_NOT_ENABLED(rv)) {
        ABTS_NOT_IMPL(tc, "IPv6 not enabled");
        return;
    }
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_PTR_NOTNULL(tc, sock);
    apr_socket_close(sock);
#else
    ABTS_NOT_IMPL(tc, "IPv6");
#endif
}

static void sendto_receivefrom(abts_case *tc, void *data)
{
    apr_status_t rv;
    apr_socket_t *sock = NULL;
    apr_socket_t *sock2 = NULL;
    char sendbuf[STRLEN] = "APR_INET, SOCK_DGRAM";
    char recvbuf[80];
    char *ip_addr;
    apr_port_t fromport;
    apr_sockaddr_t *from;
    apr_sockaddr_t *to;
    apr_size_t len = 30;
    int family;
    const char *addr;

#if APR_HAVE_IPV6
    family = APR_INET6;
    addr = "::1";
    rv = apr_socket_create(&sock, family, SOCK_DGRAM, 0, p);
    if (V6_NOT_ENABLED(rv)) {
#endif
        family = APR_INET;
        addr = "127.0.0.1";
        rv = apr_socket_create(&sock, family, SOCK_DGRAM, 0, p);
#if APR_HAVE_IPV6
    } 
#endif
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = apr_socket_create(&sock2, family, SOCK_DGRAM, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = apr_sockaddr_info_get(&to, addr, APR_UNSPEC, 7772, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = apr_sockaddr_info_get(&from, addr, APR_UNSPEC, 7771, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = apr_socket_opt_set(sock, APR_SO_REUSEADDR, 1);
    APR_ASSERT_SUCCESS(tc, "Could not set REUSEADDR on socket", rv);
    rv = apr_socket_opt_set(sock2, APR_SO_REUSEADDR, 1);
    APR_ASSERT_SUCCESS(tc, "Could not set REUSEADDR on socket2", rv);

    rv = apr_socket_bind(sock, to);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = apr_socket_bind(sock2, from);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    len = STRLEN;
    rv = apr_socket_sendto(sock2, to, 0, sendbuf, &len);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, STRLEN, len);

    len = 80;
    rv = apr_socket_recvfrom(from, sock, 0, recvbuf, &len);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, STRLEN, len);
    ABTS_STR_EQUAL(tc, "APR_INET, SOCK_DGRAM", recvbuf);

    apr_sockaddr_ip_get(&ip_addr, from);
    fromport = from->port;
    ABTS_STR_EQUAL(tc, addr, ip_addr);
    ABTS_INT_EQUAL(tc, 7771, fromport);

    apr_socket_close(sock);
    apr_socket_close(sock2);
}

static void socket_userdata(abts_case *tc, void *data)
{
    apr_socket_t *sock1, *sock2;
    apr_status_t rv;
    void *user;
    const char *key = "GENERICKEY";

    rv = apr_socket_create(&sock1, AF_INET, SOCK_STREAM, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = apr_socket_create(&sock2, AF_INET, SOCK_STREAM, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = apr_socket_data_set(sock1, "SOCK1", key, NULL);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    rv = apr_socket_data_set(sock2, "SOCK2", key, NULL);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = apr_socket_data_get(&user, key, sock1);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_STR_EQUAL(tc, "SOCK1", user);
    rv = apr_socket_data_get(&user, key, sock2);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_STR_EQUAL(tc, "SOCK2", user);
}

abts_suite *testsockets(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, tcp_socket, NULL);
    abts_run_test(suite, udp_socket, NULL);

    abts_run_test(suite, tcp6_socket, NULL);
    abts_run_test(suite, udp6_socket, NULL);

    abts_run_test(suite, sendto_receivefrom, NULL);

    abts_run_test(suite, socket_userdata, NULL);
    
    return suite;
}

