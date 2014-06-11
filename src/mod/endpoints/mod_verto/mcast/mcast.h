/*
 * Copyright (c) 2011, Anthony Minessale II
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 
 * * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 * 
 * * Neither the name of the original author; nor the names of any contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 * 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER
 * OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MCAST_H
#define __MCAST_H

#ifdef __cplusplus
extern "C" {
#endif /* defined(__cplusplus) */
#if EMACS_WORKS
}
#endif

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

typedef struct {
	int sock;
	unsigned char ttl;
	struct sockaddr_in send_addr;
	struct sockaddr_in recv_addr;
	unsigned char buffer[65536];
	int ready;
} mcast_handle_t;

typedef enum {
	MCAST_SEND = (1 << 0),
	MCAST_RECV = (1 << 1),
	MCAST_TTL_HOST = (1 << 2),
	MCAST_TTL_SUBNET = (1 << 3),
	MCAST_TTL_SITE = (1 << 4),
	MCAST_TTL_REGION = (1 << 5),
	MCAST_TTL_CONTINENT = (1 << 6),
	MCAST_TTL_UNIVERSE = (1 << 7)
} mcast_flag_t;

int mcast_socket_create(const char *host, int16_t port, mcast_handle_t *handle, mcast_flag_t flags);
void mcast_socket_close(mcast_handle_t *handle);
ssize_t mcast_socket_send(mcast_handle_t *handle, void *data, size_t datalen);
ssize_t mcast_socket_recv(mcast_handle_t *handle, void *data, size_t datalen, int ms);

#ifdef __cplusplus
}
#endif /* defined(__cplusplus) */

#endif
