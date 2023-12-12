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
#include "fspr_portable.h"
#include "fspr_general.h"
#include "fspr_lib.h"

static int os2_socket_init(int, int ,int);

int (*fspr_os2_socket)(int, int, int) = os2_socket_init;
int (*fspr_os2_select)(int *, int, int, int, long) = NULL;
int (*fspr_os2_sock_errno)() = NULL;
int (*fspr_os2_accept)(int, struct sockaddr *, int *) = NULL;
int (*fspr_os2_bind)(int, struct sockaddr *, int) = NULL;
int (*fspr_os2_connect)(int, struct sockaddr *, int) = NULL;
int (*fspr_os2_getpeername)(int, struct sockaddr *, int *) = NULL;
int (*fspr_os2_getsockname)(int, struct sockaddr *, int *) = NULL;
int (*fspr_os2_getsockopt)(int, int, int, char *, int *) = NULL;
int (*fspr_os2_ioctl)(int, int, caddr_t, int) = NULL;
int (*fspr_os2_listen)(int, int) = NULL;
int (*fspr_os2_recv)(int, char *, int, int) = NULL;
int (*fspr_os2_send)(int, const char *, int, int) = NULL;
int (*fspr_os2_setsockopt)(int, int, int, char *, int) = NULL;
int (*fspr_os2_shutdown)(int, int) = NULL;
int (*fspr_os2_soclose)(int) = NULL;
int (*fspr_os2_writev)(int, struct iovec *, int) = NULL;
int (*fspr_os2_sendto)(int, const char *, int, int, const struct sockaddr *, int);
int (*fspr_os2_recvfrom)(int, char *, int, int, struct sockaddr *, int *);

static HMODULE hSO32DLL;

static int os2_fn_link()
{
    DosEnterCritSec(); /* Stop two threads doing this at the same time */

    if (fspr_os2_socket == os2_socket_init) {
        ULONG rc;
        char errorstr[200];

        rc = DosLoadModule(errorstr, sizeof(errorstr), "SO32DLL", &hSO32DLL);

        if (rc)
            return APR_OS2_STATUS(rc);

        rc = DosQueryProcAddr(hSO32DLL, 0, "SOCKET", &fspr_os2_socket);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "SELECT", &fspr_os2_select);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "SOCK_ERRNO", &fspr_os2_sock_errno);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "ACCEPT", &fspr_os2_accept);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "BIND", &fspr_os2_bind);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "CONNECT", &fspr_os2_connect);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "GETPEERNAME", &fspr_os2_getpeername);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "GETSOCKNAME", &fspr_os2_getsockname);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "GETSOCKOPT", &fspr_os2_getsockopt);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "IOCTL", &fspr_os2_ioctl);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "LISTEN", &fspr_os2_listen);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "RECV", &fspr_os2_recv);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "SEND", &fspr_os2_send);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "SETSOCKOPT", &fspr_os2_setsockopt);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "SHUTDOWN", &fspr_os2_shutdown);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "SOCLOSE", &fspr_os2_soclose);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "WRITEV", &fspr_os2_writev);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "SENDTO", &fspr_os2_sendto);

        if (!rc)
            rc = DosQueryProcAddr(hSO32DLL, 0, "RECVFROM", &fspr_os2_recvfrom);

        if (rc)
            return APR_OS2_STATUS(rc);
    }

    DosExitCritSec();
    return APR_SUCCESS;
}



static int os2_socket_init(int domain, int type, int protocol)
{
    int rc = os2_fn_link();
    if (rc == APR_SUCCESS)
        return fspr_os2_socket(domain, type, protocol);
    return rc;
}
