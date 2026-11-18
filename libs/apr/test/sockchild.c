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

#include <stdlib.h>
#include "testsock.h"
#include "fspr_network_io.h"
#include "fspr_pools.h"

int main(int argc, char *argv[])
{
    fspr_pool_t *p;
    fspr_socket_t *sock;
    fspr_status_t rv;
    fspr_sockaddr_t *remote_sa;

    fspr_initialize();
    atexit(fspr_terminate);
    fspr_pool_create(&p, NULL);

    if (argc < 2) {
        exit(-1);
    }

    rv = fspr_sockaddr_info_get(&remote_sa, "127.0.0.1", APR_UNSPEC, 8021, 0, p);
    if (rv != APR_SUCCESS) {
        exit(-1);
    }

    if (fspr_socket_create(&sock, remote_sa->family, SOCK_STREAM, 0,
                p) != APR_SUCCESS) {
        exit(-1);
    }

    rv = fspr_socket_timeout_set(sock, fspr_time_from_sec(3));
    if (rv) {
        exit(-1);
    }

    fspr_socket_connect(sock, remote_sa);
        
    if (!strcmp("read", argv[1])) {
        char datarecv[STRLEN];
        fspr_size_t length = STRLEN;
        fspr_status_t rv;

        memset(datarecv, 0, STRLEN);
        rv = fspr_socket_recv(sock, datarecv, &length);
        fspr_socket_close(sock);
        if (APR_STATUS_IS_TIMEUP(rv)) {
            exit(SOCKET_TIMEOUT); 
        }

        if (strcmp(datarecv, DATASTR)) {
            exit(-1);
        }
        
        exit(length);
    }
    else if (!strcmp("write", argv[1])) {
        fspr_size_t length = strlen(DATASTR);
        fspr_socket_send(sock, DATASTR, &length);

        fspr_socket_close(sock);
        exit(length);
    }
    exit(-1);
}
