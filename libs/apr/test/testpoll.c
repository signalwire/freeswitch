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
#include "fspr_strings.h"
#include "fspr_errno.h"
#include "fspr_general.h"
#include "fspr_lib.h"
#include "fspr_network_io.h"
#include "fspr_poll.h"

#define SMALL_NUM_SOCKETS 3
/* We can't use 64 here, because some platforms *ahem* Solaris *ahem* have
 * a default limit of 64 open file descriptors per process.  If we use
 * 64, the test will fail even though the code is correct.
 */
#define LARGE_NUM_SOCKETS 50

static fspr_socket_t *s[LARGE_NUM_SOCKETS];
static fspr_sockaddr_t *sa[LARGE_NUM_SOCKETS];
static fspr_pollset_t *pollset;

/* ###: tests surrounded by ifdef OLD_POLL_INTERFACE either need to be
 * converted to use the pollset interface or removed. */

#ifdef OLD_POLL_INTERFACE
static fspr_pollfd_t *pollarray;
static fspr_pollfd_t *pollarray_large;
#endif

static void make_socket(fspr_socket_t **sock, fspr_sockaddr_t **sa, 
                        fspr_port_t port, fspr_pool_t *p, abts_case *tc)
{
    fspr_status_t rv;

    rv = fspr_sockaddr_info_get(sa, "127.0.0.1", APR_UNSPEC, port, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_socket_create(sock, (*sa)->family, SOCK_DGRAM, 0, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv =fspr_socket_bind((*sock), (*sa));
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
}

#ifdef OLD_POLL_INTERFACE
static void check_sockets(const fspr_pollfd_t *pollarray, 
                          fspr_socket_t **sockarray, int which, int pollin, 
                          abts_case *tc)
{
    fspr_status_t rv;
    fspr_int16_t event;
    char *str;

    rv = fspr_poll_revents_get(&event, sockarray[which], 
                              (fspr_pollfd_t *)pollarray);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    if (pollin) {
        str = fspr_psprintf(p, "Socket %d not signalled when it should be",
                           which);
        ABTS_ASSERT(tc, str, event & APR_POLLIN);
    } else {
        str = fspr_psprintf(p, "Socket %d signalled when it should not be",
                           which);
        ABTS_ASSERT(tc, str, !(event & APR_POLLIN));
    }
}
#endif

static void send_msg(fspr_socket_t **sockarray, fspr_sockaddr_t **sas, int which,
                     abts_case *tc)
{
    fspr_size_t len = 5;
    fspr_status_t rv;

    ABTS_PTR_NOTNULL(tc, sockarray[which]);

    rv = fspr_socket_sendto(sockarray[which], sas[which], 0, "hello", &len);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, strlen("hello"), len);
}

static void recv_msg(fspr_socket_t **sockarray, int which, fspr_pool_t *p, 
                     abts_case *tc)
{
    fspr_size_t buflen = 5;
    char *buffer = fspr_pcalloc(p, sizeof(char) * (buflen + 1));
    fspr_sockaddr_t *recsa;
    fspr_status_t rv;

    ABTS_PTR_NOTNULL(tc, sockarray[which]);

    fspr_sockaddr_info_get(&recsa, "127.0.0.1", APR_UNSPEC, 7770, 0, p);

    rv = fspr_socket_recvfrom(recsa, sockarray[which], 0, buffer, &buflen);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, strlen("hello"), buflen);
    ABTS_STR_EQUAL(tc, "hello", buffer);
}

    
static void create_all_sockets(abts_case *tc, void *data)
{
    int i;

    for (i = 0; i < LARGE_NUM_SOCKETS; i++){
        make_socket(&s[i], &sa[i], 7777 + i, p, tc);
    }
}
       
#ifdef OLD_POLL_INTERFACE
static void setup_small_poll(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int i;

    rv = fspr_poll_setup(&pollarray, SMALL_NUM_SOCKETS, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    
    for (i = 0; i < SMALL_NUM_SOCKETS;i++){
        ABTS_INT_EQUAL(tc, 0, pollarray[i].reqevents);
        ABTS_INT_EQUAL(tc, 0, pollarray[i].rtnevents);

        rv = fspr_poll_socket_add(pollarray, s[i], APR_POLLIN);
        ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
        ABTS_PTR_EQUAL(tc, s[i], pollarray[i].desc.s);
    }
}

static void setup_large_poll(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int i;

    rv = fspr_poll_setup(&pollarray_large, LARGE_NUM_SOCKETS, p);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    
    for (i = 0; i < LARGE_NUM_SOCKETS;i++){
        ABTS_INT_EQUAL(tc, 0, pollarray_large[i].reqevents);
        ABTS_INT_EQUAL(tc, 0, pollarray_large[i].rtnevents);

        rv = fspr_poll_socket_add(pollarray_large, s[i], APR_POLLIN);
        ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
        ABTS_PTR_EQUAL(tc, s[i], pollarray_large[i].desc.s);
    }
}

static void nomessage(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int srv = SMALL_NUM_SOCKETS;

    rv = fspr_poll(pollarray, SMALL_NUM_SOCKETS, &srv, 2 * APR_USEC_PER_SEC);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_TIMEUP(rv));
    check_sockets(pollarray, s, 0, 0, tc);
    check_sockets(pollarray, s, 1, 0, tc);
    check_sockets(pollarray, s, 2, 0, tc);
}

static void send_2(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int srv = SMALL_NUM_SOCKETS;

    send_msg(s, sa, 2, tc);

    rv = fspr_poll(pollarray, SMALL_NUM_SOCKETS, &srv, 2 * APR_USEC_PER_SEC);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    check_sockets(pollarray, s, 0, 0, tc);
    check_sockets(pollarray, s, 1, 0, tc);
    check_sockets(pollarray, s, 2, 1, tc);
}

static void recv_2_send_1(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int srv = SMALL_NUM_SOCKETS;

    recv_msg(s, 2, p, tc);
    send_msg(s, sa, 1, tc);

    rv = fspr_poll(pollarray, SMALL_NUM_SOCKETS, &srv, 2 * APR_USEC_PER_SEC);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    check_sockets(pollarray, s, 0, 0, tc);
    check_sockets(pollarray, s, 1, 1, tc);
    check_sockets(pollarray, s, 2, 0, tc);
}

static void send_2_signaled_1(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int srv = SMALL_NUM_SOCKETS;

    send_msg(s, sa, 2, tc);

    rv = fspr_poll(pollarray, SMALL_NUM_SOCKETS, &srv, 2 * APR_USEC_PER_SEC);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    check_sockets(pollarray, s, 0, 0, tc);
    check_sockets(pollarray, s, 1, 1, tc);
    check_sockets(pollarray, s, 2, 1, tc);
}

static void recv_1_send_0(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int srv = SMALL_NUM_SOCKETS;

    recv_msg(s, 1, p, tc);
    send_msg(s, sa, 0, tc);

    rv = fspr_poll(pollarray, SMALL_NUM_SOCKETS, &srv, 2 * APR_USEC_PER_SEC);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    check_sockets(pollarray, s, 0, 1, tc);
    check_sockets(pollarray, s, 1, 0, tc);
    check_sockets(pollarray, s, 2, 1, tc);
}

static void clear_all_signalled(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int srv = SMALL_NUM_SOCKETS;

    recv_msg(s, 0, p, tc);
    recv_msg(s, 2, p, tc);

    rv = fspr_poll(pollarray, SMALL_NUM_SOCKETS, &srv, 2 * APR_USEC_PER_SEC);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_TIMEUP(rv));
    check_sockets(pollarray, s, 0, 0, tc);
    check_sockets(pollarray, s, 1, 0, tc);
    check_sockets(pollarray, s, 2, 0, tc);
}

static void send_large_pollarray(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int lrv = LARGE_NUM_SOCKETS;
    int i;

    send_msg(s, sa, LARGE_NUM_SOCKETS - 1, tc);

    rv = fspr_poll(pollarray_large, LARGE_NUM_SOCKETS, &lrv, 
                  2 * APR_USEC_PER_SEC);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    for (i = 0; i < LARGE_NUM_SOCKETS; i++) {
        if (i == (LARGE_NUM_SOCKETS - 1)) {
            check_sockets(pollarray_large, s, i, 1, tc);
        }
        else {
            check_sockets(pollarray_large, s, i, 0, tc);
        }
    }
}

static void recv_large_pollarray(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int lrv = LARGE_NUM_SOCKETS;
    int i;

    recv_msg(s, LARGE_NUM_SOCKETS - 1, p, tc);

    rv = fspr_poll(pollarray_large, LARGE_NUM_SOCKETS, &lrv, 
                  2 * APR_USEC_PER_SEC);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_TIMEUP(rv));

    for (i = 0; i < LARGE_NUM_SOCKETS; i++) {
        check_sockets(pollarray_large, s, i, 0, tc);
    }
}
#endif

static void setup_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    rv = fspr_pollset_create(&pollset, LARGE_NUM_SOCKETS, p, 0);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
}

static void multi_event_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_pollfd_t socket_pollfd;
    int lrv;
    const fspr_pollfd_t *descs = NULL;

    ABTS_PTR_NOTNULL(tc, s[0]);
    socket_pollfd.desc_type = APR_POLL_SOCKET;
    socket_pollfd.reqevents = APR_POLLIN | APR_POLLOUT;
    socket_pollfd.desc.s = s[0];
    socket_pollfd.client_data = s[0];
    rv = fspr_pollset_add(pollset, &socket_pollfd);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    send_msg(s, sa, 0, tc);

    rv = fspr_pollset_poll(pollset, 0, &lrv, &descs);
    ABTS_INT_EQUAL(tc, 0, APR_STATUS_IS_TIMEUP(rv));
    if (lrv == 1) {
        ABTS_PTR_EQUAL(tc, s[0], descs[0].desc.s);
        ABTS_INT_EQUAL(tc, APR_POLLIN | APR_POLLOUT, descs[0].rtnevents);
        ABTS_PTR_EQUAL(tc, s[0],  descs[0].client_data);
    }
    else if (lrv == 2) {
        ABTS_PTR_EQUAL(tc, s[0], descs[0].desc.s);
        ABTS_PTR_EQUAL(tc, s[0], descs[0].client_data);
        ABTS_PTR_EQUAL(tc, s[0], descs[1].desc.s);
        ABTS_PTR_EQUAL(tc, s[0], descs[1].client_data);
        ABTS_ASSERT(tc, "returned events incorrect",
                    ((descs[0].rtnevents | descs[1].rtnevents)
                     == (APR_POLLIN | APR_POLLOUT))
                    && descs[0].rtnevents != descs[1].rtnevents);
    }
    else {
        ABTS_ASSERT(tc, "either one or two events returned",
                    lrv == 1 || lrv == 2);
    }

    recv_msg(s, 0, p, tc);

    rv = fspr_pollset_poll(pollset, 0, &lrv, &descs);
    ABTS_INT_EQUAL(tc, 0, APR_STATUS_IS_TIMEUP(rv));
    ABTS_INT_EQUAL(tc, 1, lrv);
    ABTS_PTR_EQUAL(tc, s[0], descs[0].desc.s);
    ABTS_INT_EQUAL(tc, APR_POLLOUT, descs[0].rtnevents);
    ABTS_PTR_EQUAL(tc, s[0],  descs[0].client_data);

    rv = fspr_pollset_remove(pollset, &socket_pollfd);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
}
                         
static void add_sockets_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int i;

    for (i = 0; i < LARGE_NUM_SOCKETS;i++){
        fspr_pollfd_t socket_pollfd;

        ABTS_PTR_NOTNULL(tc, s[i]);

        socket_pollfd.desc_type = APR_POLL_SOCKET;
        socket_pollfd.reqevents = APR_POLLIN;
        socket_pollfd.desc.s = s[i];
        socket_pollfd.client_data = s[i];
        rv = fspr_pollset_add(pollset, &socket_pollfd);
        ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    }
}

static void nomessage_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int lrv;
    const fspr_pollfd_t *descs = NULL;

    rv = fspr_pollset_poll(pollset, 0, &lrv, &descs);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_TIMEUP(rv));
    ABTS_INT_EQUAL(tc, 0, lrv);
    ABTS_PTR_EQUAL(tc, NULL, descs);
}

static void send0_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    const fspr_pollfd_t *descs = NULL;
    int num;
    
    send_msg(s, sa, 0, tc);
    rv = fspr_pollset_poll(pollset, 0, &num, &descs);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, 1, num);
    ABTS_PTR_NOTNULL(tc, descs);

    ABTS_PTR_EQUAL(tc, s[0], descs[0].desc.s);
    ABTS_PTR_EQUAL(tc, s[0],  descs[0].client_data);
}

static void recv0_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int lrv;
    const fspr_pollfd_t *descs = NULL;

    recv_msg(s, 0, p, tc);
    rv = fspr_pollset_poll(pollset, 0, &lrv, &descs);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_TIMEUP(rv));
    ABTS_INT_EQUAL(tc, 0, lrv);
    ABTS_PTR_EQUAL(tc, NULL, descs);
}

static void send_middle_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    const fspr_pollfd_t *descs = NULL;
    int num;
    
    send_msg(s, sa, 2, tc);
    send_msg(s, sa, 5, tc);
    rv = fspr_pollset_poll(pollset, 0, &num, &descs);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, 2, num);
    ABTS_PTR_NOTNULL(tc, descs);

    ABTS_ASSERT(tc, "Incorrect socket in result set",
            ((descs[0].desc.s == s[2]) && (descs[1].desc.s == s[5])) ||
            ((descs[0].desc.s == s[5]) && (descs[1].desc.s == s[2])));
}

static void clear_middle_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int lrv;
    const fspr_pollfd_t *descs = NULL;

    recv_msg(s, 2, p, tc);
    recv_msg(s, 5, p, tc);

    rv = fspr_pollset_poll(pollset, 0, &lrv, &descs);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_TIMEUP(rv));
    ABTS_INT_EQUAL(tc, 0, lrv);
    ABTS_PTR_EQUAL(tc, NULL, descs);
}

static void send_last_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    const fspr_pollfd_t *descs = NULL;
    int num;
    
    send_msg(s, sa, LARGE_NUM_SOCKETS - 1, tc);
    rv = fspr_pollset_poll(pollset, 0, &num, &descs);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, 1, num);
    ABTS_PTR_NOTNULL(tc, descs);

    ABTS_PTR_EQUAL(tc, s[LARGE_NUM_SOCKETS - 1], descs[0].desc.s);
    ABTS_PTR_EQUAL(tc, s[LARGE_NUM_SOCKETS - 1],  descs[0].client_data);
}

static void clear_last_pollset(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int lrv;
    const fspr_pollfd_t *descs = NULL;

    recv_msg(s, LARGE_NUM_SOCKETS - 1, p, tc);

    rv = fspr_pollset_poll(pollset, 0, &lrv, &descs);
    ABTS_INT_EQUAL(tc, 1, APR_STATUS_IS_TIMEUP(rv));
    ABTS_INT_EQUAL(tc, 0, lrv);
    ABTS_PTR_EQUAL(tc, NULL, descs);
}

static void close_all_sockets(abts_case *tc, void *data)
{
    fspr_status_t rv;
    int i;

    for (i = 0; i < LARGE_NUM_SOCKETS; i++){
        rv = fspr_socket_close(s[i]);
        ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    }
}

static void pollset_remove(abts_case *tc, void *data)
{
    fspr_status_t rv;
    fspr_pollset_t *pollset;
    const fspr_pollfd_t *hot_files;
    fspr_pollfd_t pfd;
    fspr_int32_t num;

    rv = fspr_pollset_create(&pollset, 5, p, 0);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    pfd.p = p;
    pfd.desc_type = APR_POLL_SOCKET;
    pfd.reqevents = APR_POLLOUT;

    pfd.desc.s = s[0];
    pfd.client_data = (void *)1;
    rv = fspr_pollset_add(pollset, &pfd);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    pfd.desc.s = s[1];
    pfd.client_data = (void *)2;
    rv = fspr_pollset_add(pollset, &pfd);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    pfd.desc.s = s[2];
    pfd.client_data = (void *)3;
    rv = fspr_pollset_add(pollset, &pfd);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    pfd.desc.s = s[3];
    pfd.client_data = (void *)4;
    rv = fspr_pollset_add(pollset, &pfd);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    rv = fspr_pollset_poll(pollset, 1000, &num, &hot_files);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, 4, num);

    /* now remove the pollset element referring to desc s[1] */
    pfd.desc.s = s[1];
    pfd.client_data = (void *)999; /* not used on this call */
    rv = fspr_pollset_remove(pollset, &pfd);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    /* this time only three should match */
    rv = fspr_pollset_poll(pollset, 1000, &num, &hot_files);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, 3, num);
    ABTS_PTR_EQUAL(tc, (void *)1, hot_files[0].client_data);
    ABTS_PTR_EQUAL(tc, s[0], hot_files[0].desc.s);
    ABTS_PTR_EQUAL(tc, (void *)3, hot_files[1].client_data);
    ABTS_PTR_EQUAL(tc, s[2], hot_files[1].desc.s);
    ABTS_PTR_EQUAL(tc, (void *)4, hot_files[2].client_data);
    ABTS_PTR_EQUAL(tc, s[3], hot_files[2].desc.s);
    
    /* now remove the pollset elements referring to desc s[2] */
    pfd.desc.s = s[2];
    pfd.client_data = (void *)999; /* not used on this call */
    rv = fspr_pollset_remove(pollset, &pfd);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);

    /* this time only two should match */
    rv = fspr_pollset_poll(pollset, 1000, &num, &hot_files);
    ABTS_INT_EQUAL(tc, APR_SUCCESS, rv);
    ABTS_INT_EQUAL(tc, 2, num);
    ABTS_ASSERT(tc, "Incorrect socket in result set",
            ((hot_files[0].desc.s == s[0]) && (hot_files[1].desc.s == s[3]))  ||
            ((hot_files[0].desc.s == s[3]) && (hot_files[1].desc.s == s[0])));
    ABTS_ASSERT(tc, "Incorrect client data in result set",
            ((hot_files[0].client_data == (void *)1) &&
             (hot_files[1].client_data == (void *)4)) ||
            ((hot_files[0].client_data == (void *)4) &&
             (hot_files[1].client_data == (void *)1)));
}

abts_suite *testpoll(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    abts_run_test(suite, create_all_sockets, NULL);

#ifdef OLD_POLL_INTERFACE
    abts_run_test(suite, setup_small_poll, NULL);
    abts_run_test(suite, setup_large_poll, NULL);
    abts_run_test(suite, nomessage, NULL);
    abts_run_test(suite, send_2, NULL);
    abts_run_test(suite, recv_2_send_1, NULL);
    abts_run_test(suite, send_2_signaled_1, NULL);
    abts_run_test(suite, recv_1_send_0, NULL);
    abts_run_test(suite, clear_all_signalled, NULL);
    abts_run_test(suite, send_large_pollarray, NULL);
    abts_run_test(suite, recv_large_pollarray, NULL);
#endif

    abts_run_test(suite, setup_pollset, NULL);
    abts_run_test(suite, multi_event_pollset, NULL);
    abts_run_test(suite, add_sockets_pollset, NULL);
    abts_run_test(suite, nomessage_pollset, NULL);
    abts_run_test(suite, send0_pollset, NULL);
    abts_run_test(suite, recv0_pollset, NULL);
    abts_run_test(suite, send_middle_pollset, NULL);
    abts_run_test(suite, clear_middle_pollset, NULL);
    abts_run_test(suite, send_last_pollset, NULL);
    abts_run_test(suite, clear_last_pollset, NULL);

    abts_run_test(suite, pollset_remove, NULL);
    
    abts_run_test(suite, close_all_sockets, NULL);

    return suite;
}

