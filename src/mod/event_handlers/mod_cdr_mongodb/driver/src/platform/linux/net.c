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

/* Implementation for Linux version of net.h */
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

static int mongo_create_socket( mongo *conn ) {
    int fd;

    if( ( fd = socket( AF_INET, SOCK_STREAM, 0 ) ) == -1 ) {
        conn->err = MONGO_CONN_NO_SOCKET;
        return MONGO_ERROR;
    }
    conn->sock = fd;

    return MONGO_OK;
}

static int mongo_set_blocking_status( mongo *conn ) {
    int flags;
    int blocking;

    blocking = ( conn->conn_timeout_ms == 0 );
    if( blocking )
        return MONGO_OK;
    else {
        if( ( flags = fcntl( conn->sock, F_GETFL ) ) == -1 ) {
            conn->err = MONGO_IO_ERROR;
            mongo_close_socket( conn->sock );
            return MONGO_ERROR;
        }

        flags |= O_NONBLOCK;

        if( ( flags = fcntl( conn->sock, F_SETFL, flags ) ) == -1 ) {
            conn->err = MONGO_IO_ERROR;
            mongo_close_socket( conn->sock );
            return MONGO_ERROR;
        }
    }

    return MONGO_OK;
}

int mongo_set_socket_op_timeout( mongo *conn, int millis ) {
    struct timeval tv;
    tv.tv_sec = millis / 1000;
    tv.tv_usec = ( millis % 1000 ) * 1000;

    if ( setsockopt( conn->sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof( tv ) ) == -1 ) {
        conn->err = MONGO_IO_ERROR;
        return MONGO_ERROR;
    }

    if ( setsockopt( conn->sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof( tv ) ) == -1 ) {
        conn->err = MONGO_IO_ERROR;
        return MONGO_ERROR;
    }

    return MONGO_OK;
}

#ifdef _MONGO_USE_GETADDRINFO
int mongo_socket_connect( mongo *conn, const char *host, int port ) {

    struct addrinfo *addrs = NULL;
    struct addrinfo hints;
    int flag = 1;
    char port_str[12];
    int ret;

    conn->sock = 0;
    conn->connected = 0;

    memset( &hints, 0, sizeof( hints ) );
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    sprintf( port_str, "%d", port );

    if( mongo_create_socket( conn ) != MONGO_OK )
        return MONGO_ERROR;

    if( getaddrinfo( host, port_str, &hints, &addrs ) != 0 ) {
        bson_errprintf( "getaddrinfo failed: %s", gai_strerror( ret ) );
        conn->err = MONGO_CONN_ADDR_FAIL;
        return MONGO_ERROR;
    }

    if ( connect( conn->sock, addrs->ai_addr, addrs->ai_addrlen ) == -1 ) {
        mongo_close_socket( conn->sock );
        freeaddrinfo( addrs );
        conn->err = MONGO_CONN_FAIL;
        return MONGO_ERROR;
    }

    setsockopt( conn->sock, IPPROTO_TCP, TCP_NODELAY, ( char * )&flag, sizeof( flag ) );
    if( conn->op_timeout_ms > 0 )
        mongo_set_socket_op_timeout( conn, conn->op_timeout_ms );

    conn->connected = 1;
    freeaddrinfo( addrs );

    return MONGO_OK;
}
#else
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
#endif
