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

#ifndef _KS_SOCKET_H_
#define _KS_SOCKET_H_

#include "ks.h"

#ifndef WIN32
#include <poll.h>
#endif

KS_BEGIN_EXTERN_C

#define KS_SO_NONBLOCK 2999

#ifdef WIN32

static __inline int ks_errno(void)
{
	return WSAGetLastError();
}

static __inline int ks_errno_is_blocking(int errcode)
{
	return errcode == WSAEWOULDBLOCK || errcode == WSAEINPROGRESS || errcode == 35 || errcode == 730035;
}

static __inline int ks_errno_is_interupt(int errcode)
{
	return 0;
}

#else

static inline int ks_errno(void)
{
	return errno;
}

static inline int ks_errno_is_blocking(int errcode)
{
  return errcode == EAGAIN || errcode == EWOULDBLOCK || errcode == EINPROGRESS || errcode == EINTR || errcode == ETIMEDOUT || errcode == 35 || errcode == 730035;
}

static inline int ks_errno_is_interupt(int errcode)
{
	return errcode == EINTR;
}

#endif

static __inline int ks_socket_valid(ks_socket_t s) {
	return s != KS_SOCK_INVALID;
}

#define KS_SA_INIT {AF_INET};

KS_DECLARE(ks_status_t) ks_socket_send(ks_socket_t sock, void *data, ks_size_t *datalen);
KS_DECLARE(ks_status_t) ks_socket_recv(ks_socket_t sock, void *data, ks_size_t *datalen);
KS_DECLARE(ks_status_t) ks_socket_sendto(ks_socket_t sock, void *data, ks_size_t *datalen, ks_sockaddr_t *addr);
KS_DECLARE(ks_status_t) ks_socket_recvfrom(ks_socket_t sock, void *data, ks_size_t *datalen, ks_sockaddr_t *addr);

typedef struct pollfd *ks_ppollfd_t;
KS_DECLARE(int) ks_poll(ks_ppollfd_t fds, uint32_t nfds, int timeout);
KS_DECLARE(ks_status_t) ks_socket_option(ks_socket_t socket, int option_name, ks_bool_t enabled);
KS_DECLARE(ks_status_t) ks_socket_sndbuf(ks_socket_t socket, int bufsize);
KS_DECLARE(ks_status_t) ks_socket_rcvbuf(ks_socket_t socket, int bufsize);
KS_DECLARE(int) ks_wait_sock(ks_socket_t sock, uint32_t ms, ks_poll_t flags);

KS_DECLARE(ks_socket_t) ks_socket_connect(int type, int protocol, ks_sockaddr_t *addr);
KS_DECLARE(ks_status_t) ks_addr_bind(ks_socket_t server_sock, ks_sockaddr_t *addr);
KS_DECLARE(const char *) ks_addr_get_host(ks_sockaddr_t *addr);
KS_DECLARE(ks_port_t) ks_addr_get_port(ks_sockaddr_t *addr);
KS_DECLARE(int) ks_addr_cmp(const ks_sockaddr_t *sa1, const ks_sockaddr_t *sa2);
KS_DECLARE(ks_status_t) ks_addr_copy(ks_sockaddr_t *addr, const ks_sockaddr_t *src_addr);
KS_DECLARE(ks_status_t) ks_addr_set(ks_sockaddr_t *addr, const char *host, ks_port_t port, int family);
KS_DECLARE(ks_status_t) ks_addr_set_raw(ks_sockaddr_t *addr, void *data, ks_port_t port, int family);
KS_DECLARE(ks_status_t) ks_addr_raw_data(const ks_sockaddr_t *addr, void **data, ks_size_t *datalen);
KS_DECLARE(ks_status_t) ks_listen(const char *host, ks_port_t port, int family, int backlog, ks_listen_callback_t callback, void *user_data);
KS_DECLARE(ks_status_t) ks_socket_shutdown(ks_socket_t sock, int how);
KS_DECLARE(ks_status_t) ks_socket_close(ks_socket_t *sock);
KS_DECLARE(ks_status_t) ks_ip_route(char *buf, int len, const char *route_ip);
KS_DECLARE(ks_status_t) ks_find_local_ip(char *buf, int len, int *mask, int family, const char *route_ip);
KS_DECLARE(ks_status_t) ks_listen_sock(ks_socket_t server_sock, ks_sockaddr_t *addr, int backlog, ks_listen_callback_t callback, void *user_data);
KS_END_EXTERN_C

#endif /* defined(_KS_SOCKET_H_) */

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
