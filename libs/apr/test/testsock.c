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

#include "testutil.h"
#include "testsock.h"
#include "fspr_thread_proc.h"
#include "fspr_network_io.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_strings.h"
#include "fspr_poll.h"

static void launch_child(abts_case *tc, fspr_proc_t *proc, const char *arg1, fspr_pool_t *p)
{
    fspr_procattr_t *procattr;
    const char *args[3];
    fspr_status_t rv;

    rv = fspr_procattr_create(&procattr, p);
    APR_ASSERT_SUCCESS(tc, "Couldn't create procattr", rv);

    rv = fspr_procattr_io_set(procattr, APR_NO_PIPE, APR_NO_PIPE,
            APR_NO_PIPE);
    APR_ASSERT_SUCCESS(tc, "Couldn't set io in procattr", rv);

    rv = fspr_procattr_error_check_set(procattr, 1);
    APR_ASSERT_SUCCESS(tc, "Couldn't set error check in procattr", rv);

    args[0] = "sockchild" EXTENSION;
    args[1] = arg1;
    args[2] = NULL;
    rv = fspr_proc_create(proc, "./sockchild" EXTENSION, args, NULL,
                         procattr, p);
    APR_ASSERT_SUCCESS(tc, "Couldn't launch program", rv);
}

static int wait_child(abts_case *tc, fspr_proc_t *proc) 
{
    int exitcode;
    fspr_exit_why_e why;

    ABTS_ASSERT(tc, "Error waiting for child process",
            fspr_proc_wait(proc, &exitcode, &why, APR_WAIT) == APR_CHILD_DONE);

    ABTS_ASSERT(tc, "child terminated normally", why == APR_PROC_EXIT);
    return exitcode;
}

static void test_addr_info(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_sockaddr_t *sa;

    rv = fspr_sockaddr_info_get(&sa, NULL, APR_UNSPEC, 80, 0, p);
    APR_ASSERT_SUCCESS(tc, "Problem generating sockaddr", rv);

    rv = fspr_sockaddr_info_get(&sa, "127.0.0.1", APR_UNSPEC, 80, 0, p);
    APR_ASSERT_SUCCESS(tc, "Problem generating sockaddr", rv);
    ABTS_STR_EQUAL(tc, "127.0.0.1", sa->hostname);
}

static fspr_socket_t *setup_socket(abts_case *tc)
{
    fspr_status_t rv;
    fspr_sockaddr_t *sa;
    fspr_socket_t *sock;

    rv = fspr_sockaddr_info_get(&sa, "127.0.0.1", APR_INET, 8021, 0, p);
    APR_ASSERT_SUCCESS(tc, "Problem generating sockaddr", rv);

    rv = fspr_socket_create(&sock, sa->family, SOCK_STREAM, APR_PROTO_TCP, p);
    APR_ASSERT_SUCCESS(tc, "Problem creating socket", rv);

    rv = fspr_socket_opt_set(sock, APR_SO_REUSEADDR, 1);
    APR_ASSERT_SUCCESS(tc, "Could not set REUSEADDR on socket", rv);
    
    rv = fspr_socket_bind(sock, sa);
    APR_ASSERT_SUCCESS(tc, "Problem binding to port", rv);
    if (rv) return NULL;
                
    rv = fspr_socket_listen(sock, 5);
    APR_ASSERT_SUCCESS(tc, "Problem listening on socket", rv);

    return sock;
}

static void test_create_bind_listen(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_socket_t *sock = setup_socket(tc);
    
    if (!sock) return;
    
    rv = fspr_socket_close(sock);
    APR_ASSERT_SUCCESS(tc, "Problem closing socket", rv);
}

static void test_send(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_socket_t *sock;
    fspr_socket_t *sock2;
    fspr_proc_t proc;
    int protocol;
    fspr_size_t length;

    sock = setup_socket(tc);
    if (!sock) return;

    launch_child(tc, &proc, "read", p);
    
    rv = fspr_socket_accept(&sock2, sock, p);
    APR_ASSERT_SUCCESS(tc, "Problem with receiving connection", rv);

    fspr_socket_protocol_get(sock2, &protocol);
    ABTS_INT_EQUAL(tc, APR_PROTO_TCP, protocol);
    
    length = strlen(DATASTR);
    fspr_socket_send(sock2, DATASTR, &length);

    /* Make sure that the client received the data we sent */
    ABTS_INT_EQUAL(tc, strlen(DATASTR), wait_child(tc, &proc));

    rv = fspr_socket_close(sock2);
    APR_ASSERT_SUCCESS(tc, "Problem closing connected socket", rv);
    rv = fspr_socket_close(sock);
    APR_ASSERT_SUCCESS(tc, "Problem closing socket", rv);
}

static void test_recv(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_socket_t *sock;
    fspr_socket_t *sock2;
    fspr_proc_t proc;
    int protocol;
    fspr_size_t length = STRLEN;
    char datastr[STRLEN];
    
    sock = setup_socket(tc);
    if (!sock) return;

    launch_child(tc, &proc, "write", p);
    
    rv = fspr_socket_accept(&sock2, sock, p);
    APR_ASSERT_SUCCESS(tc, "Problem with receiving connection", rv);

    fspr_socket_protocol_get(sock2, &protocol);
    ABTS_INT_EQUAL(tc, APR_PROTO_TCP, protocol);
    
    memset(datastr, 0, STRLEN);
    fspr_socket_recv(sock2, datastr, &length);

    /* Make sure that the server received the data we sent */
    ABTS_STR_EQUAL(tc, DATASTR, datastr);
    ABTS_INT_EQUAL(tc, strlen(datastr), wait_child(tc, &proc));

    rv = fspr_socket_close(sock2);
    APR_ASSERT_SUCCESS(tc, "Problem closing connected socket", rv);
    rv = fspr_socket_close(sock);
    APR_ASSERT_SUCCESS(tc, "Problem closing socket", rv);
}

static void test_timeout(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_socket_t *sock;
    fspr_socket_t *sock2;
    fspr_proc_t proc;
    int protocol;
    int exit;
    
    sock = setup_socket(tc);
    if (!sock) return;

    launch_child(tc, &proc, "read", p);
    
    rv = fspr_socket_accept(&sock2, sock, p);
    APR_ASSERT_SUCCESS(tc, "Problem with receiving connection", rv);

    fspr_socket_protocol_get(sock2, &protocol);
    ABTS_INT_EQUAL(tc, APR_PROTO_TCP, protocol);
    
    exit = wait_child(tc, &proc);    
    ABTS_INT_EQUAL(tc, SOCKET_TIMEOUT, exit);

    /* We didn't write any data, so make sure the child program returns
     * an error.
     */
    rv = fspr_socket_close(sock2);
    APR_ASSERT_SUCCESS(tc, "Problem closing connected socket", rv);
    rv = fspr_socket_close(sock);
    APR_ASSERT_SUCCESS(tc, "Problem closing socket", rv);
}

static void test_get_addr(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_socket_t *ld, *sd, *cd;
    fspr_sockaddr_t *sa, *ca;
    char a[128], b[128];

    ld = setup_socket(tc);

    APR_ASSERT_SUCCESS(tc,
                       "get local address of bound socket",
                       fspr_socket_addr_get(&sa, APR_LOCAL, ld));

    rv = fspr_socket_create(&cd, sa->family, SOCK_STREAM,
                           APR_PROTO_TCP, p);
    APR_ASSERT_SUCCESS(tc, "create client socket", rv);

    APR_ASSERT_SUCCESS(tc, "enable non-block mode",
                       fspr_socket_opt_set(cd, APR_SO_NONBLOCK, 1));

    /* It is valid for a connect() on a socket with NONBLOCK set to
     * succeed (if the connection can be established synchronously),
     * but if it does, this test cannot proceed.  */
    rv = fspr_socket_connect(cd, sa);
    if (rv == APR_SUCCESS) {
        fspr_socket_close(ld);
        fspr_socket_close(cd);
        ABTS_NOT_IMPL(tc, "Cannot test if connect completes "
                      "synchronously");
        return;
    }

    if (!APR_STATUS_IS_EINPROGRESS(rv)) {
        fspr_socket_close(ld);
        fspr_socket_close(cd);
        APR_ASSERT_SUCCESS(tc, "connect to listener", rv);
        return;
    }

    APR_ASSERT_SUCCESS(tc, "accept connection",
                       fspr_socket_accept(&sd, ld, p));
    
    {
        /* wait for writability */
        fspr_pollfd_t pfd;
        int n;

        pfd.p = p;
        pfd.desc_type = APR_POLL_SOCKET;
        pfd.reqevents = APR_POLLOUT|APR_POLLHUP;
        pfd.desc.s = cd;
        pfd.client_data = NULL;

        APR_ASSERT_SUCCESS(tc, "poll for connect completion",
                           fspr_poll(&pfd, 1, &n, 5 * APR_USEC_PER_SEC));

    }

    APR_ASSERT_SUCCESS(tc, "get local address of server socket",
                       fspr_socket_addr_get(&sa, APR_LOCAL, sd));

    APR_ASSERT_SUCCESS(tc, "get remote address of client socket",
                       fspr_socket_addr_get(&ca, APR_REMOTE, cd));
    
    fspr_snprintf(a, sizeof(a), "%pI", sa);
    fspr_snprintf(b, sizeof(b), "%pI", ca);

    ABTS_STR_EQUAL(tc, a, b);
                       
    fspr_socket_close(cd);
    fspr_socket_close(sd);
    fspr_socket_close(ld);
}

abts_suite *testsock(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, test_addr_info, NULL);
    abts_run_test(suite, test_create_bind_listen, NULL);
    abts_run_test(suite, test_send, NULL);
    abts_run_test(suite, test_recv, NULL);
    abts_run_test(suite, test_timeout, NULL);
    abts_run_test(suite, test_get_addr, NULL);

    return suite;
}

