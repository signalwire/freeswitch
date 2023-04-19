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

#include "fspr_errno.h"
#include <sys/types.h>
#include <sys/socket.h>

extern int (*fspr_os2_socket)(int, int, int);
extern int (*fspr_os2_select)(int *, int, int, int, long);
extern int (*fspr_os2_sock_errno)();
extern int (*fspr_os2_accept)(int, struct sockaddr *, int *);
extern int (*fspr_os2_bind)(int, struct sockaddr *, int);
extern int (*fspr_os2_connect)(int, struct sockaddr *, int);
extern int (*fspr_os2_getpeername)(int, struct sockaddr *, int *);
extern int (*fspr_os2_getsockname)(int, struct sockaddr *, int *);
extern int (*fspr_os2_getsockopt)(int, int, int, char *, int *);
extern int (*fspr_os2_ioctl)(int, int, caddr_t, int);
extern int (*fspr_os2_listen)(int, int);
extern int (*fspr_os2_recv)(int, char *, int, int);
extern int (*fspr_os2_send)(int, const char *, int, int);
extern int (*fspr_os2_setsockopt)(int, int, int, char *, int);
extern int (*fspr_os2_shutdown)(int, int);
extern int (*fspr_os2_soclose)(int);
extern int (*fspr_os2_writev)(int, struct iovec *, int);
extern int (*fspr_os2_sendto)(int, const char *, int, int, const struct sockaddr *, int);
extern int (*fspr_os2_recvfrom)(int, char *, int, int, struct sockaddr *, int *);

#define socket fspr_os2_socket
#define select fspr_os2_select
#define sock_errno fspr_os2_sock_errno
#define accept fspr_os2_accept
#define bind fspr_os2_bind
#define connect fspr_os2_connect
#define getpeername fspr_os2_getpeername
#define getsockname fspr_os2_getsockname
#define getsockopt fspr_os2_getsockopt
#define ioctl fspr_os2_ioctl
#define listen fspr_os2_listen
#define recv fspr_os2_recv
#define send fspr_os2_send
#define setsockopt fspr_os2_setsockopt
#define shutdown fspr_os2_shutdown
#define soclose fspr_os2_soclose
#define writev fspr_os2_writev
#define sendto fspr_os2_sendto
#define recvfrom fspr_os2_recvfrom
