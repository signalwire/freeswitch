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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "mcast.h"
#include <poll.h>

int mcast_socket_create(const char *host, int16_t port, mcast_handle_t *handle, mcast_flag_t flags)
{
	uint32_t one = 1;

	memset(handle, 0, sizeof(*handle));
	
	if ((!(flags & MCAST_SEND) && !(flags & MCAST_RECV)) || (handle->sock = socket(AF_INET, SOCK_DGRAM, 0)) <= 0 ) {
		return -1;
	}
	
	handle->send_addr.sin_family = AF_INET;
	handle->send_addr.sin_addr.s_addr = inet_addr(host);
	handle->send_addr.sin_port = htons(port);
	
	if ( setsockopt(handle->sock, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one)) != 0 ) {
		close(handle->sock);
		return -1;
	}
	

	if ((flags & MCAST_RECV)) {
		struct ip_mreq mreq;

		handle->recv_addr.sin_family = AF_INET;
		handle->recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		handle->recv_addr.sin_port = htons(port);

		mreq.imr_multiaddr.s_addr = inet_addr(host);
		mreq.imr_interface.s_addr = htonl(INADDR_ANY);

		if (setsockopt(handle->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
			close(handle->sock);
			handle->sock = -1;
			return -1;
		}

		if (bind(handle->sock, (struct sockaddr *) &handle->recv_addr, sizeof(handle->recv_addr)) < 0) {
			close(handle->sock);
			handle->sock = -1;
			return -1;
		}
		

	}

	handle->ttl = 1;

	if ((flags & MCAST_TTL_HOST)) {
		handle->ttl = 0;
	}

	if ((flags & MCAST_TTL_SUBNET)) {
		handle->ttl = 1;
	}

	if ((flags & MCAST_TTL_SITE)) {
		handle->ttl = 32;
	}

	if ((flags & MCAST_TTL_REGION)) {
		handle->ttl = 64;
	}

	if ((flags & MCAST_TTL_CONTINENT)) {
		handle->ttl = 128;
	}
	
	if ((flags & MCAST_TTL_UNIVERSE)) {
		handle->ttl = 255;
	}

	if ( setsockopt(handle->sock, IPPROTO_IP, IP_MULTICAST_TTL, &handle->ttl, sizeof(handle->ttl)) != 0 ) {
		return -1;
	}

	handle->ready = 1;
	
	return 0;
}


void mcast_socket_close(mcast_handle_t *handle)
{
	if (handle->sock > -1) {
		close(handle->sock);
		handle->sock = -1;
	}
}

ssize_t mcast_socket_send(mcast_handle_t *handle, void *data, size_t datalen)
{
	if (handle->sock <= -1) {
		return -1;
	}

	if (data == NULL || datalen == 0) {
		data = handle->buffer;
		datalen = sizeof(handle->buffer);
	}

	return sendto(handle->sock, data, datalen, 0, (struct sockaddr *) &handle->send_addr, sizeof(handle->send_addr));
}

ssize_t mcast_socket_recv(mcast_handle_t *handle, void *data, size_t datalen, int ms)
{
	socklen_t addrlen = sizeof(handle->recv_addr);
	int r;

	if (data == NULL || datalen == 0) {
		data = handle->buffer;
		datalen = sizeof(handle->buffer);
	}

	if (ms > 0) {
		struct pollfd pfds[1];
		
		pfds[0].fd = handle->sock;
		pfds[0].events = POLLIN|POLLERR;
		
		if ((r = poll(pfds, 1, ms)) <= 0) {
			return r;
		}

		if (pfds[0].revents & POLLERR) {
			return -1;
		}
	}

	
	return recvfrom(handle->sock, data, datalen, 0, (struct sockaddr *) &handle->recv_addr, &addrlen);
}
