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
#ifdef WIN32
#include <ws2tcpip.h>
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <time.h>
#ifndef WIN32
#include <unistd.h>
#endif
#ifndef WIN32
#include <poll.h>
#define closesocket(x) close(x)
#endif
#include <switch_utils.h>
#include "mcast.h"


int mcast_socket_create(const char *host, int16_t port, mcast_handle_t *handle, mcast_flag_t flags)
{
	uint32_t one = 1;
	int family = AF_INET;

	memset(handle, 0, sizeof(*handle));
	
	if (strchr(host, ':')) { 
		family = AF_INET6;
	}
	
	if ((!(flags & MCAST_SEND) && !(flags & MCAST_RECV)) || (handle->sock = (mcast_socket_t)socket(family, SOCK_DGRAM, 0)) != mcast_sock_invalid ) {
		return -1;
	}

	if (family == AF_INET6) {
		handle->send_addr6.sin6_family = AF_INET6;
		handle->send_addr6.sin6_port = htons(port);
		inet_pton(AF_INET6, host, &(handle->send_addr6.sin6_addr));
		handle->family = AF_INET6;
	} else {
		handle->send_addr.sin_family = AF_INET;
		handle->send_addr.sin_addr.s_addr = inet_addr(host);
		handle->send_addr.sin_port = htons(port);
		handle->family = AF_INET;
	}
	
	if ( setsockopt(handle->sock, SOL_SOCKET, SO_REUSEADDR, (void *)&one, sizeof(one)) != 0 ) {
		mcast_socket_close(handle);
		return -1;
	}
	

	if ((flags & MCAST_RECV)) {
		if (handle->family == AF_INET) {
			struct ip_mreq mreq;
			
			handle->recv_addr.sin_family = AF_INET;
			handle->recv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
			handle->recv_addr.sin_port = htons(port);

			mreq.imr_multiaddr.s_addr = inet_addr(host);
			mreq.imr_interface.s_addr = htonl(INADDR_ANY);

			if (setsockopt(handle->sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void *)&mreq, sizeof(mreq)) < 0) {
				mcast_socket_close(handle);
				return -1;
			}

			if (bind(handle->sock, (struct sockaddr *) &handle->recv_addr, sizeof(handle->recv_addr)) < 0) {
				mcast_socket_close(handle);
				return -1;
			}

		} else {
			struct ipv6_mreq mreq;
			struct addrinfo addr_criteria;
			struct addrinfo *mcast_addr;
			char service[80] = "";
			
			memset(&addr_criteria, 0, sizeof(addr_criteria));
			addr_criteria.ai_family = AF_UNSPEC;
			addr_criteria.ai_socktype = SOCK_DGRAM;
			addr_criteria.ai_protocol = IPPROTO_UDP;
			addr_criteria.ai_flags |= AI_NUMERICHOST;
			
			snprintf(service, sizeof(service), "%d", port);
			getaddrinfo(host, service, &addr_criteria, &mcast_addr);

			
			memset(&handle->recv_addr6, 0, sizeof(handle->recv_addr6));
			handle->recv_addr6.sin6_family = AF_INET6;
			handle->recv_addr6.sin6_port = htons(port);
			inet_pton(AF_INET6, "::0", &(handle->recv_addr6.sin6_addr));

			memcpy(&mreq.ipv6mr_multiaddr, &((struct sockaddr_in6 *)mcast_addr->ai_addr)->sin6_addr,  sizeof(struct in6_addr));
											 
			mreq.ipv6mr_interface = 0;
			setsockopt(handle->sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, (const char *)&mreq, sizeof(mreq));

			if (bind(handle->sock, (struct sockaddr *) &handle->recv_addr6, sizeof(handle->recv_addr6)) < 0) {
				mcast_socket_close(handle);
				return -1;
			}
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

	if ( setsockopt(handle->sock, IPPROTO_IP, IP_MULTICAST_TTL, (void *)&handle->ttl, sizeof(handle->ttl)) != 0 ) {
		return -1;
	}

	handle->ready = 1;
	
	return 0;
}


void mcast_socket_close(mcast_handle_t *handle)
{
	if (handle->sock != mcast_sock_invalid) {
		closesocket(handle->sock);
		handle->sock = mcast_sock_invalid;
	}
}

ssize_t mcast_socket_send(mcast_handle_t *handle, void *data, size_t datalen)
{
	if (handle->sock != mcast_sock_invalid) {
		return -1;
	}

	if (data == NULL || datalen == 0) {
		data = handle->buffer;
		datalen = sizeof(handle->buffer);
	}

	if (handle->family == AF_INET6) {
		return sendto(handle->sock, data, (int)datalen, 0, (struct sockaddr *) &handle->send_addr6, sizeof(handle->send_addr6));
	} else {
		return sendto(handle->sock, data, (int)datalen, 0, (struct sockaddr *) &handle->send_addr, sizeof(handle->send_addr));
	}
}

ssize_t mcast_socket_recv(mcast_handle_t *handle, void *data, size_t datalen, int ms)
{
	socklen_t addrlen = sizeof(handle->recv_addr);

	if (data == NULL || datalen == 0) {
		data = handle->buffer;
		datalen = sizeof(handle->buffer);
	}

	if (ms > 0) {
		int pflags = switch_wait_sock(handle->sock, ms, SWITCH_POLL_READ | SWITCH_POLL_ERROR | SWITCH_POLL_HUP);

		if ((pflags & SWITCH_POLL_ERROR) || (pflags & SWITCH_POLL_HUP)) {
			return -1;
		}
	}

	if (handle->family == AF_INET6) {
		return recvfrom(handle->sock, data, (int)datalen, 0, (struct sockaddr *) &handle->recv_addr6, &addrlen);
	} else {
		return recvfrom(handle->sock, data, (int)datalen, 0, (struct sockaddr *) &handle->recv_addr, &addrlen);
	}
}
