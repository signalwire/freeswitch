/** @file net.h */

/*    Copyright 2009-2011 10gen Inc.
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

/* Header for generic net.h */
#ifndef _MONGO_NET_H_
#define _MONGO_NET_H_

#include "mongo.h"

#ifdef _WIN32
#include <windows.h>
#include <winsock.h>
#define mongo_close_socket(sock) ( closesocket(sock) )
typedef int socklen_t;
#else
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#define mongo_close_socket(sock) ( close(sock) )
#endif

#ifndef _WIN32
#include <unistd.h>
#endif

#if defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE) || _POSIX_C_SOURCE >= 1
#define _MONGO_USE_GETADDRINFO
#endif

MONGO_EXTERN_C_START

/* This is a no-op in the generic implementation. */
int mongo_set_socket_op_timeout( mongo *conn, int millis );
int mongo_read_socket( mongo *conn, void *buf, int len );
int mongo_write_socket( mongo *conn, const void *buf, int len );
int mongo_socket_connect( mongo *conn, const char *host, int port );

MONGO_EXTERN_C_END
#endif
