/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2006 Nokia Corporation.
 *
 * Contact: Pekka Pessi <pekka.pessi@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

/**@CFILE tport_type_tcp.c TCP Transport
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Mar 24 08:45:49 EET 2006 ppessi
 */

#include "config.h"

#include "tport_internal.h"

#if HAVE_NETINET_TCP_H
#include <netinet/tcp.h>
#endif

#ifndef SOL_TCP
#define SOL_TCP IPPROTO_TCP
#endif

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "tport_type_tcp";
#endif

/* ---------------------------------------------------------------------- */
/* TCP */

tport_vtable_t const tport_tcp_vtable =
{
  "tcp", tport_type_local,
  sizeof (tport_primary_t),
  tport_tcp_init_primary,
  NULL,
  tport_accept,
  NULL,
  sizeof (tport_t),
  tport_tcp_init_secondary,
  NULL,
  NULL,
  NULL,
  NULL,
  tport_recv_stream,
  tport_send_stream,
};

tport_vtable_t const tport_tcp_client_vtable =
{
  "tcp", tport_type_client,
  sizeof (tport_primary_t),
  tport_tcp_init_client,
  NULL,
  tport_accept,
  NULL,
  sizeof (tport_t),
  tport_tcp_init_secondary,
  NULL,
  NULL,
  NULL,
  NULL,
  tport_recv_stream,
  tport_send_stream,
};

static int tport_tcp_setsndbuf(int socket, int atleast);

int tport_tcp_init_primary(tport_primary_t *pri, 
			   tp_name_t tpn[1],
			   su_addrinfo_t *ai,
			   tagi_t const *tags,
			   char const **return_culprit)
{
  int socket;

  socket = su_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

  if (socket == INVALID_SOCKET)
    return *return_culprit = "socket", -1;

  tport_tcp_setsndbuf(socket, 64 * 1024);

  return tport_stream_init_primary(pri, socket, tpn, ai, tags, return_culprit);
}

int tport_stream_init_primary(tport_primary_t *pri, 
			      su_socket_t socket,
			      tp_name_t tpn[1],
			      su_addrinfo_t *ai,
			      tagi_t const *tags,
			      char const **return_culprit)
{
  pri->pri_primary->tp_socket = socket;

  /* Set IP TOS if set */
  tport_set_tos(socket, ai, pri->pri_params->tpp_tos);

#if defined(__linux__)
  /* Linux does not allow reusing TCP port while this one is open,
     so we can safely call su_setreuseaddr() before bind(). */
  su_setreuseaddr(socket, 1);
#endif

  if (tport_bind_socket(socket, ai, return_culprit) == -1)
    return -1;

  if (listen(socket, pri->pri_params->tpp_qsize) == SOCKET_ERROR)
    return *return_culprit = "listen", -1;

#if !defined(__linux__)
  /* Allow reusing TCP sockets
   *
   * On Solaris & BSD, call setreuseaddr() after bind in order to avoid
   * binding to a port owned by an existing server.
   */
  su_setreuseaddr(socket, 1);
#endif

  pri->pri_primary->tp_events = SU_WAIT_ACCEPT;
  pri->pri_primary->tp_conn_orient = 1;

  return 0;
}

int tport_tcp_init_client(tport_primary_t *pri, 
			  tp_name_t tpn[1],
			  su_addrinfo_t *ai,
			  tagi_t const *tags,
			  char const **return_culprit)
{
  pri->pri_primary->tp_conn_orient = 1;

  return 0;
}

int tport_tcp_init_secondary(tport_t *self, int socket, int accepted,
			     char const **return_reason)
{
  int one = 1;

  self->tp_has_connection = 1;

  if (setsockopt(socket, SOL_TCP, TCP_NODELAY, (void *)&one, sizeof one) == -1)
    return *return_reason = "TCP_NODELAY", -1;
  if (su_setblocking(socket, 0) < 0)
    return *return_reason = "su_setblocking", -1;

  if (!accepted)
    tport_tcp_setsndbuf(socket, 64 * 1024);

  return 0;
}

static int tport_tcp_setsndbuf(int socket, int atleast)
{
#if SU_HAVE_WINSOCK2
  /* Set send buffer size to something reasonable on windows */
  int size = 0;
  socklen_t sizelen = sizeof size;

  if (getsockopt(socket, SOL_SOCKET, SO_SNDBUF, (void *)&size, &sizelen) < 0)
    return -1;

  if (sizelen != sizeof size)
    return su_seterrno(EINVAL);

  if (size >= atleast)
    return 0;			/* OK */

  return setsockopt(socket, SOL_SOCKET, SO_SNDBUF,
		    (void *)&atleast, sizeof atleast);
#else
  return 0;
#endif
}

/** Receive from stream.
 *
 * @retval -1 error
 * @retval 0  end-of-stream  
 * @retval 1  normal receive
 * @retval 2  incomplete recv, recv again
 * 
 */
int tport_recv_stream(tport_t *self)
{
  msg_t *msg;
  ssize_t n, N, veclen;
  int err;
  msg_iovec_t iovec[msg_n_fragments] = {{ 0 }};

  N = su_getmsgsize(self->tp_socket);
  if (N == 0) {
    if (self->tp_msg)
      msg_recv_commit(self->tp_msg, 0, 1);
    return 0;    /* End of stream */
  }
  if (N == -1) {
    err = su_errno();
    SU_DEBUG_1(("%s(%p): su_getmsgsize(): %s (%d)\n", __func__, (void *)self,
		su_strerror(err), err));
    return -1;
  }

  veclen = tport_recv_iovec(self, &self->tp_msg, iovec, N, 0);
  if (veclen == -1)
    return -1;

  msg = self->tp_msg;

  msg_set_address(msg, self->tp_addr, (socklen_t)(self->tp_addrlen));

  n = su_vrecv(self->tp_socket, iovec, veclen, 0, NULL, NULL);
  if (n == SOCKET_ERROR)
    return tport_recv_error_report(self);

  assert(n <= N);

  /* Write the received data to the message dump file */
  if (self->tp_master->mr_dump_file)
    tport_dump_iovec(self, msg, n, iovec, veclen, "recv", "from");

  /* Mark buffer as used */
  msg_recv_commit(msg, n, 0);

  return 1;
}

ssize_t tport_send_stream(tport_t const *self, msg_t *msg, 
			  msg_iovec_t iov[], 
			  size_t iovused)
{
#if __sun__			/* XXX - there must be a better way... */
  if (iovused > 16)
    iovused = 16;
#endif
  return su_vsend(self->tp_socket, iov, iovused, MSG_NOSIGNAL, NULL, 0);
}
