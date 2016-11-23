/*
 * Copyright (c) 2007-2014, Anthony Minessale II
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


/* Use select on windows and poll everywhere else.
   Select is the devil.  Especially if you are doing a lot of small socket connections.
   If your FD number is bigger than 1024 you will silently create memory corruption.

   If you have build errors on your platform because you don't have poll find a way to detect it and #define KS_USE_SELECT and #undef KS_USE_POLL
   All of this will be upgraded to autoheadache eventually.
*/

/* TBD for win32 figure out how to tell if you have WSAPoll (vista or higher) and use it when available by #defining KS_USE_WSAPOLL (see below) */

#ifdef _MSC_VER
#define FD_SETSIZE 8192
//#define KS_USE_SELECT
#else
#define KS_USE_POLL
#endif

#include <ks.h>

#ifndef WIN32
#define closesocket(s) close(s)
#else /* WIN32 */

#pragma warning (disable:6386)
/* These warnings need to be ignored warning in sdk header */
#include <Ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")

#ifndef errno
#define errno WSAGetLastError()
#endif

#ifndef EINTR
#define EINTR WSAEINTR
#endif

#pragma warning (default:6386)

#endif /* WIN32 */

#ifdef KS_USE_POLL
#include <poll.h>
#endif

KS_DECLARE(ks_status_t) ks_socket_option(ks_socket_t socket, int option_name, ks_bool_t enabled)
{
	int result = -1;
	ks_status_t status = KS_STATUS_FAIL;
#ifdef WIN32
	BOOL opt = TRUE;
	if (!enabled) opt = FALSE;
#else
	int opt = 1;
	if (!enabled) opt = 0;
#endif

	switch(option_name) {
	case SO_REUSEADDR:
	case TCP_NODELAY:
	case SO_KEEPALIVE:
	case SO_LINGER:
#ifdef WIN32
		result = setsockopt(socket, SOL_SOCKET, option_name, (char *) &opt, sizeof(opt));
#else
		result = setsockopt(socket, SOL_SOCKET, option_name, &opt, sizeof(opt));
#endif
		if (!result) status = KS_STATUS_SUCCESS;
		break;
	case KS_SO_NONBLOCK:
		{
#ifdef WIN32
			u_long val = (u_long)!!opt;
			if (ioctlsocket(socket, FIONBIO, &val) != SOCKET_ERROR) {
				status = KS_STATUS_SUCCESS;
			}
#else
			int flags = fcntl(socket, F_GETFL, 0);
			if (opt) {
				flags |= O_NONBLOCK;
			} else {
				flags &= ~O_NONBLOCK;
			}
			if (fcntl(socket, F_SETFL, flags) != -1) {
				status = KS_STATUS_SUCCESS;
			}
#endif
		}
		break;
	default:
		break;
	}

	return status;
}

KS_DECLARE(ks_status_t) ks_socket_sndbuf(ks_socket_t socket, int bufsize)
{
	int result;
	ks_status_t status = KS_STATUS_FAIL;

#ifdef WIN32
	result = setsockopt(socket, SOL_SOCKET, SO_SNDBUF, (char *) &bufsize, sizeof(bufsize));
#else
	result = setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
#endif
	if (!result) status = KS_STATUS_SUCCESS;

	return status;
}

KS_DECLARE(ks_status_t) ks_socket_rcvbuf(ks_socket_t socket, int bufsize)
{
	int result;
	ks_status_t status = KS_STATUS_FAIL;

#ifdef WIN32
	result = setsockopt(socket, SOL_SOCKET, SO_RCVBUF, (char *) &bufsize, sizeof(bufsize));
#else
	result = setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
#endif
	if (!result) status = KS_STATUS_SUCCESS;

	return status;
}

static int ks_socket_reuseaddr(ks_socket_t socket)
{
#ifdef WIN32
	BOOL reuse_addr = TRUE;
	return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, (char *) &reuse_addr, sizeof(reuse_addr));
#else
	int reuse_addr = 1;
	return setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr));
#endif
}

KS_DECLARE(ks_status_t) ks_socket_shutdown(ks_socket_t sock, int how)
{
	return shutdown(sock, how) ? KS_STATUS_FAIL : KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_socket_close(ks_socket_t *sock)
{
	ks_assert(sock);

	if (*sock != KS_SOCK_INVALID) {
		closesocket(*sock);
		*sock = KS_SOCK_INVALID;
		return KS_STATUS_SUCCESS;
	}

	return KS_STATUS_FAIL;
}

KS_DECLARE(ks_socket_t) ks_socket_connect(int type, int protocol, ks_sockaddr_t *addr)
{
	ks_socket_t sock = KS_SOCK_INVALID;

	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);

	if ((sock = socket(addr->family, type, protocol)) == KS_SOCK_INVALID) {
		return KS_SOCK_INVALID;
	}

	if (addr->family == AF_INET) {
		if (connect(sock, (struct sockaddr *)&addr->v.v4, sizeof(addr->v.v4))) {
			ks_socket_close(&sock);
			return KS_SOCK_INVALID;
		}
	} else {
		if (connect(sock, (struct sockaddr *)&addr->v.v6, sizeof(addr->v.v6))) {
			ks_socket_close(&sock);
			return KS_SOCK_INVALID;
		}
	}

	return sock;
}

KS_DECLARE(ks_status_t) ks_addr_bind(ks_socket_t server_sock, ks_sockaddr_t *addr)
{
	ks_status_t status = KS_STATUS_SUCCESS;

	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);

	if (addr->family == AF_INET) {
		if (bind(server_sock, (struct sockaddr *) &addr->v.v4, sizeof(addr->v.v4)) < 0) {
			status = KS_STATUS_FAIL;
		}
	} else {
		if (bind(server_sock, (struct sockaddr *) &addr->v.v6, sizeof(addr->v.v6)) < 0) {
			status = KS_STATUS_FAIL;
		}
	}
	
	return status;
}

KS_DECLARE(const char *) ks_addr_get_host(ks_sockaddr_t *addr)
{
	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);

	if (addr->family == AF_INET) {
		inet_ntop(AF_INET, &addr->v.v4.sin_addr, addr->host, sizeof(addr->host));
	} else {
		inet_ntop(AF_INET6, &addr->v.v6.sin6_addr, addr->host, sizeof(addr->host));
	}

	return (const char *) addr->host;
}

KS_DECLARE(ks_port_t) ks_addr_get_port(ks_sockaddr_t *addr)
{
	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);

	if (addr->family == AF_INET) {
		addr->port = ntohs(addr->v.v4.sin_port);
	} else {
		addr->port = ntohs(addr->v.v6.sin6_port);
	}

	return addr->port;
}

KS_DECLARE(int) ks_addr_cmp(const ks_sockaddr_t *sa1, const ks_sockaddr_t *sa2)
{

	if (!(sa1 && sa2)) {
		return 0;
	}

	if (sa1->family != sa2->family) {
		return 0;
	}

	switch (sa1->family) {
	case AF_INET:
		return (sa1->v.v4.sin_addr.s_addr == sa2->v.v4.sin_addr.s_addr && sa1->v.v4.sin_port == sa2->v.v4.sin_port);
	case AF_INET6:
		{
			int i;

			if (sa1->v.v6.sin6_port != sa2->v.v6.sin6_port) {
				return 0;
			}

			for (i = 0; i < 4; i++) {
				if (*((int32_t *) &sa1->v.v6.sin6_addr + i) != *((int32_t *) &sa2->v.v6.sin6_addr + i)) {
					return 0;
				}
			}

			return 1;
		}
	}

	return 0;
}

KS_DECLARE(ks_status_t) ks_addr_copy(ks_sockaddr_t *addr, const ks_sockaddr_t *src_addr)
{
	ks_status_t status = KS_STATUS_SUCCESS;

	ks_assert(addr);
	ks_assert(src_addr);
	ks_assert(src_addr->family == AF_INET || src_addr->family == AF_INET6);

	addr->family = src_addr->family;

	if (src_addr->family == AF_INET) {
		memcpy(&addr->v.v4, &src_addr->v.v4, sizeof(src_addr->v.v4));
	} else {
		memcpy(&addr->v.v6, &src_addr->v.v6, sizeof(src_addr->v.v6));
	}

	ks_addr_get_host(addr);
	ks_addr_get_port(addr);

	return status;
}


KS_DECLARE(ks_status_t) ks_addr_set(ks_sockaddr_t *addr, const char *host, ks_port_t port, int family)
{
	ks_status_t status = KS_STATUS_SUCCESS;

	ks_assert(addr);

	if (family != PF_INET && family != PF_INET6) family = PF_INET;
	if (host && strchr(host, ':')) family = PF_INET6;

	memset(addr, 0, sizeof(*addr));

	if (family == PF_INET) {
		addr->family = AF_INET;
		addr->v.v4.sin_family = AF_INET;
		addr->v.v4.sin_addr.s_addr = host ? inet_addr(host): htonl(INADDR_ANY);
		addr->v.v4.sin_port = htons(port);
	} else {
		addr->family = AF_INET6;
		addr->v.v6.sin6_family = AF_INET6;
		addr->v.v6.sin6_port = htons(port);
		if (host) {
			inet_pton(AF_INET6, host, &(addr->v.v6.sin6_addr));
		} else {
			addr->v.v6.sin6_addr = in6addr_any;
		}
	} 

	ks_addr_get_host(addr);
	ks_addr_get_port(addr);

	return status;
}


KS_DECLARE(ks_status_t) ks_addr_set_raw(ks_sockaddr_t *addr, void *data, ks_port_t port, int family)
{
	ks_status_t status = KS_STATUS_SUCCESS;

	ks_assert(addr);
	
	if (family != PF_INET && family != PF_INET6) family = PF_INET;

	memset(addr, 0, sizeof(*addr));

	if (family == PF_INET) {
		addr->family = AF_INET;
		addr->v.v4.sin_family = AF_INET;
		memcpy(&(addr->v.v4.sin_addr), data, 4);
		addr->v.v4.sin_port = port;
	} else {
		addr->family = AF_INET6;
		addr->v.v6.sin6_family = AF_INET6;
		addr->v.v6.sin6_port = port;
		memcpy(&(addr->v.v6.sin6_addr), data, 16);
	} 

	ks_addr_get_host(addr);
	ks_addr_get_port(addr);

	return status;
}


KS_DECLARE(ks_status_t) ks_listen_sock(ks_socket_t server_sock, ks_sockaddr_t *addr, int backlog, ks_listen_callback_t callback, void *user_data)
{
	ks_status_t status = KS_STATUS_SUCCESS;


	ks_socket_reuseaddr(server_sock);
	
	if (ks_addr_bind(server_sock, addr) != KS_STATUS_SUCCESS) {
		status = KS_STATUS_FAIL;
		goto end;
	}

	if (!backlog) backlog = 10000;

	if (listen(server_sock, backlog) < 0) {
		status = KS_STATUS_FAIL;
		goto end;
	}

	for (;;) {
		ks_socket_t client_sock;
		ks_sockaddr_t remote_addr;
		socklen_t slen = 0;

		if (addr->family == PF_INET) {
			slen = sizeof(remote_addr.v.v4);
			if ((client_sock = accept(server_sock, (struct sockaddr *) &remote_addr.v.v4, &slen)) == KS_SOCK_INVALID) {
				status = KS_STATUS_FAIL;
				goto end;
			}
			remote_addr.family = AF_INET;
		} else {
			slen = sizeof(remote_addr.v.v6);
			if ((client_sock = accept(server_sock, (struct sockaddr *) &remote_addr.v.v6, &slen)) == KS_SOCK_INVALID) {
				status = KS_STATUS_FAIL;
				goto end;
			}
			remote_addr.family = AF_INET6;
		}

		ks_addr_get_host(&remote_addr);
		ks_addr_get_port(&remote_addr);

		callback(server_sock, client_sock, &remote_addr, user_data);
	}

  end:

	if (server_sock != KS_SOCK_INVALID) {
		ks_socket_shutdown(server_sock, 2);
		ks_socket_close(&server_sock);
		server_sock = KS_SOCK_INVALID;
	}

	return status;
}

KS_DECLARE(ks_status_t) ks_listen(const char *host, ks_port_t port, int family, int backlog, ks_listen_callback_t callback, void *user_data)
{
	ks_socket_t server_sock = KS_SOCK_INVALID;
	ks_sockaddr_t addr = { 0 };

	if (family != PF_INET && family != PF_INET6) family = PF_INET;
	if (host && strchr(host, ':')) family = PF_INET6;

	if (ks_addr_set(&addr, host, port, family) != KS_STATUS_SUCCESS) {
		return KS_STATUS_FAIL;
	}
	
	if ((server_sock = socket(family, SOCK_STREAM, IPPROTO_TCP)) == KS_SOCK_INVALID) {
		return KS_STATUS_FAIL;
	}

	return ks_listen_sock(server_sock, &addr, backlog, callback, user_data);
}

KS_DECLARE(int) ks_poll(struct pollfd fds[], uint32_t nfds, int timeout)
{
#ifdef WIN32
	return WSAPoll(fds, nfds, timeout);
#else
	return poll(fds, nfds, timeout);
#endif
}

#ifdef KS_USE_SELECT
#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 6262 )	/* warning C6262: Function uses '98348' bytes of stack: exceeds /analyze:stacksize'16384'. Consider moving some data to heap */
#endif
KS_DECLARE(int) ks_wait_sock(ks_socket_t sock, uint32_t ms, ks_poll_t flags)
{
	int s = 0, r = 0;
	fd_set rfds;
	fd_set wfds;
	fd_set efds;
	struct timeval tv;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);
	FD_ZERO(&efds);

#ifndef WIN32
	/* Wouldn't you rather know?? */
	assert(sock <= FD_SETSIZE);
#endif

	if ((flags & KS_POLL_READ)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
#pragma warning( disable : 4548 )
		FD_SET(sock, &rfds);
#pragma warning( pop )
#else
		FD_SET(sock, &rfds);
#endif
	}

	if ((flags & KS_POLL_WRITE)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
#pragma warning( disable : 4548 )
		FD_SET(sock, &wfds);
#pragma warning( pop )
#else
		FD_SET(sock, &wfds);
#endif
	}

	if ((flags & KS_POLL_ERROR)) {

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4127 )
#pragma warning( disable : 4548 )
		FD_SET(sock, &efds);
#pragma warning( pop )
#else
		FD_SET(sock, &efds);
#endif
	}

	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * ms;

	s = select((int)sock + 1, (flags & KS_POLL_READ) ? &rfds : NULL, (flags & KS_POLL_WRITE) ? &wfds : NULL, (flags & KS_POLL_ERROR) ? &efds : NULL, &tv);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((flags & KS_POLL_READ) && FD_ISSET(sock, &rfds)) {
			r |= KS_POLL_READ;
		}

		if ((flags & KS_POLL_WRITE) && FD_ISSET(sock, &wfds)) {
			r |= KS_POLL_WRITE;
		}

		if ((flags & KS_POLL_ERROR) && FD_ISSET(sock, &efds)) {
			r |= KS_POLL_ERROR;
		}
	}

	return r;

}

#ifdef WIN32
#pragma warning( pop )
#endif
#endif

#if defined(KS_USE_POLL) || defined(WIN32)
KS_DECLARE(int) ks_wait_sock(ks_socket_t sock, uint32_t ms, ks_poll_t flags)
{
	struct pollfd pfds[2] = { {0} };
	int s = 0, r = 0;

	pfds[0].fd = sock;

	if ((flags & KS_POLL_READ)) {
		pfds[0].events |= POLLIN;
	}

	if ((flags & KS_POLL_WRITE)) {
		pfds[0].events |= POLLOUT;
	}

	if ((flags & KS_POLL_ERROR)) {
		pfds[0].events |= POLLERR;
	}

	s = ks_poll(pfds, 1, ms);

	if (s < 0) {
		r = s;
	} else if (s > 0) {
		if ((pfds[0].revents & POLLIN)) {
			r |= KS_POLL_READ;
		}
		if ((pfds[0].revents & POLLOUT)) {
			r |= KS_POLL_WRITE;
		}
		if ((pfds[0].revents & POLLERR)) {
			r |= KS_POLL_ERROR;
		}
	}

	return r;

}
#endif


#ifdef HAVE_GETIFADDRS
#include <ifaddrs.h>
static int get_netmask(struct sockaddr_in *me, int *mask)
{
	struct ifaddrs *ifaddrs, *i = NULL;

	if (!me || getifaddrs(&ifaddrs) < 0) {
		return -1;
	}

	for (i = ifaddrs; i; i = i->ifa_next) {
		struct sockaddr_in *s = (struct sockaddr_in *) i->ifa_addr;
		struct sockaddr_in *m = (struct sockaddr_in *) i->ifa_netmask;

		if (s && m && s->sin_family == AF_INET && s->sin_addr.s_addr == me->sin_addr.s_addr) {
			*mask = m->sin_addr.s_addr;
			freeifaddrs(ifaddrs);
			return 0;
		}
	}

	freeifaddrs(ifaddrs);

	return -2;
}
#elif defined(__linux__)

#include <sys/ioctl.h>
#include <net/if.h>
static int get_netmask(struct sockaddr_in *me, int *mask)
{

	static struct ifreq ifreqs[20] = { {{{0}}} };
	struct ifconf ifconf;
	int nifaces, i;
	int sock;
	int r = -1;

	memset(&ifconf, 0, sizeof(ifconf));
	ifconf.ifc_buf = (char *) (ifreqs);
	ifconf.ifc_len = sizeof(ifreqs);


	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		goto end;
	}

	if (ioctl(sock, SIOCGIFCONF, (char *) &ifconf) < 0) {
		goto end;
	}

	nifaces = ifconf.ifc_len / sizeof(struct ifreq);

	for (i = 0; i < nifaces; i++) {
		struct sockaddr_in *sin = NULL;
		struct in_addr ip;

		ioctl(sock, SIOCGIFADDR, &ifreqs[i]);
		sin = (struct sockaddr_in *) &ifreqs[i].ifr_addr;
		ip = sin->sin_addr;

		if (ip.s_addr == me->sin_addr.s_addr) {
			ioctl(sock, SIOCGIFNETMASK, &ifreqs[i]);
			sin = (struct sockaddr_in *) &ifreqs[i].ifr_addr;
			/* mask = sin->sin_addr; */
			*mask = sin->sin_addr.s_addr;
			r = 0;
			break;
		}

	}

  end:

	close(sock);
	return r;

}

#elif defined(WIN32)

static int get_netmask(struct sockaddr_in *me, int *mask)
{
	SOCKET sock = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
	INTERFACE_INFO interfaces[20];
	unsigned long bytes;
	int interface_count, x;
	int r = -1;

	*mask = 0;

	if (sock == SOCKET_ERROR) {
		return -1;
	}

	if (WSAIoctl(sock, SIO_GET_INTERFACE_LIST, 0, 0, &interfaces, sizeof(interfaces), &bytes, 0, 0) == SOCKET_ERROR) {
		r = -1;
		goto end;
	}

	interface_count = bytes / sizeof(INTERFACE_INFO);

	for (x = 0; x < interface_count; ++x) {
		struct sockaddr_in *addr = (struct sockaddr_in *) &(interfaces[x].iiAddress);

		if (addr->sin_addr.s_addr == me->sin_addr.s_addr) {
			struct sockaddr_in *netmask = (struct sockaddr_in *) &(interfaces[x].iiNetmask);
			*mask = netmask->sin_addr.s_addr;
			r = 0;
			break;
		}
	}

  end:
	closesocket(sock);
	return r;
}

#else

static int get_netmask(struct sockaddr_in *me, int *mask)
{
	return -1;
}

#endif


KS_DECLARE(ks_status_t) ks_ip_route(char *buf, int len, const char *route_ip)
{
	int family = AF_INET;

	ks_assert(route_ip);

	if (strchr(route_ip, ':')) {
		family = AF_INET6;
	}

	return ks_find_local_ip(buf, len, NULL, family, route_ip);
}

KS_DECLARE(ks_status_t) ks_find_local_ip(char *buf, int len, int *mask, int family, const char *route_ip)
{
	ks_status_t status = KS_STATUS_FAIL;
	char *base = (char *)route_ip;

#ifdef WIN32
	SOCKET tmp_socket;
	SOCKADDR_STORAGE l_address;
	int l_address_len;
	struct addrinfo *address_info = NULL;
#else
#ifdef __Darwin__
	int ilen;
#else
	unsigned int ilen;
#endif
	int tmp_socket = -1, on = 1;
	char abuf[25] = "";
#endif

	if (len < 16) {
		return status;
	}

	switch (family) {
	case AF_INET:
		ks_copy_string(buf, "127.0.0.1", len);
		if (!base) {
			base = "82.45.148.209";
		}
		break;
	case AF_INET6:
		ks_copy_string(buf, "::1", len);
		if (!base) {
			base = "2001:503:BA3E::2:30";	/* DNS Root server A */
		}
		break;
	default:
		base = "127.0.0.1";
		break;
	}

#ifdef WIN32
	tmp_socket = socket(family, SOCK_DGRAM, 0);

	getaddrinfo(base, NULL, NULL, &address_info);

	if (!address_info || WSAIoctl(tmp_socket,
								  SIO_ROUTING_INTERFACE_QUERY,
								  address_info->ai_addr, (DWORD) address_info->ai_addrlen, &l_address, sizeof(l_address), (LPDWORD) & l_address_len, NULL,
								  NULL)) {

		closesocket(tmp_socket);
		if (address_info)
			freeaddrinfo(address_info);
		return status;
	}


	closesocket(tmp_socket);
	freeaddrinfo(address_info);

	if (!getnameinfo((const struct sockaddr *) &l_address, l_address_len, buf, len, NULL, 0, NI_NUMERICHOST)) {
		status = KS_STATUS_SUCCESS;
		if (mask && family == AF_INET) {
			get_netmask((struct sockaddr_in *) &l_address, mask);
		}
	}
#else

	switch (family) {
	case AF_INET:
		{
			struct sockaddr_in iface_out;
			struct sockaddr_in remote;
			memset(&remote, 0, sizeof(struct sockaddr_in));

			remote.sin_family = AF_INET;
			remote.sin_addr.s_addr = inet_addr(base);
			remote.sin_port = htons(4242);

			memset(&iface_out, 0, sizeof(iface_out));
			if ( (tmp_socket = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ) {
				goto doh;
			}

			if (setsockopt(tmp_socket, SOL_SOCKET, SO_BROADCAST, &on, sizeof(on)) == -1) {
				goto doh;
			}

			if (connect(tmp_socket, (struct sockaddr *) &remote, sizeof(struct sockaddr_in)) == -1) {
				goto doh;
			}

			ilen = sizeof(iface_out);
			if (getsockname(tmp_socket, (struct sockaddr *) &iface_out, &ilen) == -1) {
				goto doh;
			}

			if (iface_out.sin_addr.s_addr == 0) {
				goto doh;
			}

			getnameinfo((struct sockaddr *) &iface_out, sizeof(iface_out), abuf, sizeof(abuf), NULL, 0, NI_NUMERICHOST);
			ks_copy_string(buf, abuf, len);
			
			if (mask && family == AF_INET) {
				get_netmask((struct sockaddr_in *) &iface_out, mask);
			}

			status = KS_STATUS_SUCCESS;
		}
		break;
	case AF_INET6:
		{
			struct sockaddr_in6 iface_out;
			struct sockaddr_in6 remote;
			memset(&remote, 0, sizeof(struct sockaddr_in6));

			remote.sin6_family = AF_INET6;
			inet_pton(AF_INET6, base, &remote.sin6_addr);
			remote.sin6_port = htons(4242);

			memset(&iface_out, 0, sizeof(iface_out));
			if ( (tmp_socket = socket(AF_INET6, SOCK_DGRAM, 0)) == -1 ) {
				goto doh;
			}

			if (connect(tmp_socket, (struct sockaddr *) &remote, sizeof(remote)) == -1) {
				goto doh;
			}

			ilen = sizeof(iface_out);
			if (getsockname(tmp_socket, (struct sockaddr *) &iface_out, &ilen) == -1) {
				goto doh;
			}
			
			inet_ntop(AF_INET6, (const void *) &iface_out.sin6_addr, buf, len - 1);

			status = KS_STATUS_SUCCESS;
		}
		break;
	}

  doh:
	if (tmp_socket > 0) {
		close(tmp_socket);
	}
#endif

	return status;
}


KS_DECLARE(ks_status_t) ks_addr_raw_data(const ks_sockaddr_t *addr, void **data, ks_size_t *datalen)
{
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);

	if (addr->family == AF_INET) {
		*data = (void *)&addr->v.v4.sin_addr;
		*datalen = 4;
	} else {
		*data = (void *)&addr->v.v6.sin6_addr;
		*datalen = 16;
	}

	return KS_STATUS_SUCCESS;
}

KS_DECLARE(ks_status_t) ks_socket_send(ks_socket_t sock, void *data, ks_size_t *datalen)
{
	ks_ssize_t r;
	ks_status_t status = KS_STATUS_FAIL;

	do {
#ifdef WIN32
		r = send(sock, data, (int)*datalen, 0);
#else
		r = send(sock, data, *datalen, 0);
#endif
	} while (r == -1 && ks_errno_is_interupt(ks_errno()));

	if (r > 0) {
		*datalen = (ks_size_t) r;
		status = KS_STATUS_SUCCESS;
	} else if (r == 0) {
		status = KS_STATUS_DISCONNECTED;

	} else if (ks_errno_is_blocking(ks_errno())) {
		status = KS_STATUS_BREAK;
	}

return status;
}

KS_DECLARE(ks_status_t) ks_socket_recv(ks_socket_t sock, void *data, ks_size_t *datalen)
{
	ks_ssize_t r;
	ks_status_t status = KS_STATUS_FAIL;

	do {
#ifdef WIN32
		r = recv(sock, data, (int)*datalen, 0);
#else
		r = recv(sock, data, *datalen, 0);
#endif
	} while (r == -1 && ks_errno_is_interupt(ks_errno()));

	if (r > 0) {
		*datalen = (ks_size_t) r;
		status = KS_STATUS_SUCCESS;
	} else if (r == 0) {
		status = KS_STATUS_DISCONNECTED;
	} else if (ks_errno_is_blocking(ks_errno())) {
		status = KS_STATUS_BREAK;
	}

	return status;
}

KS_DECLARE(ks_status_t) ks_socket_sendto(ks_socket_t sock, void *data, ks_size_t *datalen, ks_sockaddr_t *addr)
{
	struct sockaddr *sockaddr;
	socklen_t socksize = 0;
	ks_status_t status = KS_STATUS_FAIL;
	ks_ssize_t r;

	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);

	if (addr->family == AF_INET) {
		sockaddr = (struct sockaddr *) &addr->v.v4;
		socksize = sizeof(addr->v.v4);
	} else {
		sockaddr = (struct sockaddr *) &addr->v.v6;
		socksize = sizeof(addr->v.v6);
	}
	
	do {
#ifdef WIN32
		r = sendto(sock, data, (int)*datalen, 0, sockaddr, socksize);
#else
		r = sendto(sock, data, *datalen, 0, sockaddr, socksize);
#endif
	} while (r == -1 && ks_errno_is_interupt(ks_errno()));

	if (r > 0) {
		*datalen = (ks_size_t) r;
		status = KS_STATUS_SUCCESS;
	} else if (r == 0) {
		status = KS_STATUS_DISCONNECTED;
	} else if (ks_errno_is_blocking(ks_errno())) {
		status = KS_STATUS_BREAK;
	}

	return status;

}

KS_DECLARE(ks_status_t) ks_socket_recvfrom(ks_socket_t sock, void *data, ks_size_t *datalen, ks_sockaddr_t *addr)
{
	struct sockaddr *sockaddr;
	ks_status_t status = KS_STATUS_FAIL;
	ks_ssize_t r;
	socklen_t alen;

	ks_assert(addr);
	ks_assert(addr->family == AF_INET || addr->family == AF_INET6);

	if (addr->family == AF_INET) {
		sockaddr = (struct sockaddr *) &addr->v.v4;
		alen = sizeof(addr->v.v4);
	} else {
		sockaddr = (struct sockaddr *) &addr->v.v6;
		alen = sizeof(addr->v.v6);
	}

	do {
#ifdef WIN32
		r = recvfrom(sock, data, (int)*datalen, 0, sockaddr, &alen);
#else
		r = recvfrom(sock, data, *datalen, 0, sockaddr, &alen);
#endif
	} while (r == -1 && ks_errno_is_interupt(ks_errno()));

	if (r > 0) {
		ks_addr_get_host(addr);
		ks_addr_get_port(addr);
		*datalen = (ks_size_t) r;
		status = KS_STATUS_SUCCESS;
	} else if (r == 0) {
		status = KS_STATUS_DISCONNECTED;
	} else if (ks_errno_is_blocking(ks_errno())) {
		status = KS_STATUS_BREAK;
	}
	
	return status;
}

