/* net.c */

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

/* Implementation for generic version of net.h */
#include "net.h"
#include <string.h>

int mongo_write_socket( mongo *conn, const void *buf, int len ) {
    const char *cbuf = buf;
    while ( len ) {
        int sent = send( conn->sock, cbuf, len, 0 );
        if ( sent == -1 ) {
            conn->err = MONGO_IO_ERROR;
            return MONGO_ERROR;
        }
        cbuf += sent;
        len -= sent;
    }

    return MONGO_OK;
}

int mongo_read_socket( mongo *conn, void *buf, int len ) {
    char *cbuf = buf;
    while ( len ) {
        int sent = recv( conn->sock, cbuf, len, 0 );
        if ( sent == 0 || sent == -1 ) {
            conn->err = MONGO_IO_ERROR;
            return MONGO_ERROR;
        }
        cbuf += sent;
        len -= sent;
    }

    return MONGO_OK;
}

/* This is a no-op in the generic implementation. */
int mongo_set_socket_op_timeout( mongo *conn, int millis ) {
    return MONGO_OK;
}

static int mongo_create_socket( mongo *conn ) {
    int fd;

    if( ( fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 ) {
        conn->err = MONGO_CONN_NO_SOCKET;
        return MONGO_ERROR;
    }
    conn->sock = fd;

    return MONGO_OK;
}

int mongo_socket_connect( mongo *conn, const char *host, int port ) {
    struct sockaddr_in sa;
    socklen_t addressSize;
    int flag = 1;

    if( mongo_create_socket( conn ) != MONGO_OK )
        return MONGO_ERROR;

    memset( sa.sin_zero , 0 , sizeof( sa.sin_zero ) );
    sa.sin_family = AF_INET;
    sa.sin_port = htons( port );
    sa.sin_addr.s_addr = inet_addr( host );
    addressSize = sizeof( sa );

    if ( connect( conn->sock, ( struct sockaddr * )&sa, addressSize ) == -1 ) {
        mongo_close_socket( conn->sock );
        conn->connected = 0;
        conn->sock = 0;
        conn->err = MONGO_CONN_FAIL;
        return MONGO_ERROR;
    }

    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, ( char * ) &flag, sizeof( flag ) );
    if( conn->op_timeout_ms > 0 )
        mongo_set_socket_op_timeout( conn, conn->op_timeout_ms );

    conn->connected = 1;

    return MONGO_OK;
}
