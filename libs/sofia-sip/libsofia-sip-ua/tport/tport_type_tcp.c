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
#include <string.h>
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
  /* vtp_name 		     */ "tcp",
  /* vtp_public              */ tport_type_local,
  /* vtp_pri_size            */ sizeof (tport_primary_t),
  /* vtp_init_primary        */ tport_tcp_init_primary,
  /* vtp_deinit_primary      */ NULL,
  /* vtp_wakeup_pri          */ tport_accept,
  /* vtp_connect             */ NULL,
  /* vtp_secondary_size      */ sizeof (tport_t),
  /* vtp_init_secondary      */ tport_tcp_init_secondary,
  /* vtp_deinit_secondary    */ NULL,
  /* vtp_shutdown            */ NULL,
  /* vtp_set_events          */ NULL,
  /* vtp_wakeup              */ NULL,
  /* vtp_recv                */ tport_recv_stream,
  /* vtp_send                */ tport_send_stream,
  /* vtp_deliver             */ NULL,
  /* vtp_prepare             */ NULL,
  /* vtp_keepalive           */ NULL,
  /* vtp_stun_response       */ NULL,
  /* vtp_next_secondary_timer*/ tport_tcp_next_timer,
  /* vtp_secondary_timer     */ tport_tcp_timer,
};

tport_vtable_t const tport_tcp_client_vtable =
{
  /* vtp_name 		     */ "tcp",
  /* vtp_public              */ tport_type_client,
  /* vtp_pri_size            */ sizeof (tport_primary_t),
  /* vtp_init_primary        */ tport_tcp_init_client,
  /* vtp_deinit_primary      */ NULL,
  /* vtp_wakeup_pri          */ NULL,
  /* vtp_connect             */ NULL,
  /* vtp_secondary_size      */ sizeof (tport_t),
  /* vtp_init_secondary      */ tport_tcp_init_secondary,
  /* vtp_deinit_secondary    */ NULL,
  /* vtp_shutdown            */ NULL,
  /* vtp_set_events          */ NULL,
  /* vtp_wakeup              */ NULL,
  /* vtp_recv                */ tport_recv_stream,
  /* vtp_send                */ tport_send_stream,
  /* vtp_deliver             */ NULL,
  /* vtp_prepare             */ NULL,
  /* vtp_keepalive           */ NULL,
  /* vtp_stun_response       */ NULL,
  /* vtp_next_secondary_timer*/ tport_tcp_next_timer,
  /* vtp_secondary_timer     */ tport_tcp_timer,
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
  int val = 1;

  self->tp_has_connection = 1;

  if (setsockopt(socket, SOL_TCP, TCP_NODELAY, (void *)&val, sizeof val) == -1)
    return *return_reason = "TCP_NODELAY", -1;

#if defined(SO_KEEPALIVE)
  setsockopt(socket, SOL_SOCKET, SO_KEEPALIVE, (void *)&val, sizeof val);
#endif
  val = 30;
#if defined(TCP_KEEPIDLE)
  setsockopt(socket, SOL_TCP, TCP_KEEPIDLE, (void *)&val, sizeof val);
#endif
#if defined(TCP_KEEPINTVL)
  setsockopt(socket, SOL_TCP, TCP_KEEPINTVL, (void *)&val, sizeof val);
#endif

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

/** Return span of whitespace from buffer */
static inline size_t ws_span(void *buffer, size_t len)
{
  size_t i;
  char const *b = buffer;

  for (i = 0; i < len; i++) {
    if (b[i] != '\r' && b[i] != '\n' && b[i] != ' ' && b[i] != '\t')
      break;
  }

  return i;
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
  int err, initial;
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

  initial = self->tp_msg == NULL;
  memset(&self->tp_ptime, 0, sizeof self->tp_ptime);

  while (initial && N <= 8) {	/* Check for whitespace */
    char crlf[9];
    size_t i;

    n = su_recv(self->tp_socket, crlf, N, MSG_PEEK);

    i = ws_span(crlf, n);
    if (i == 0)
      break;

    n = su_recv(self->tp_socket, crlf, i, 0);
    if (n <= 0)
      return (int)n;

    SU_DEBUG_7(("%s(%p): received keepalive (total %u)\n", __func__,
		(void *)self, self->tp_ping));

    N -= n, self->tp_ping += n;

    tport_recv_bytes(self, n, n);

    if (N == 0) {
      /* outbound-10 section 3.5.1  - send pong */
      if (self->tp_ping >= 4)
	tport_tcp_pong(self);

      return 1;
    }
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

  tport_recv_bytes(self, n, n);

  /* Check if message contains only whitespace */
  /* This can happen if multiple PINGs are received at once */
  if (initial) {
    size_t i = ws_span(iovec->siv_base, iovec->siv_len);

    if (i + self->tp_ping >= 4)
      tport_tcp_pong(self);
    else

      self->tp_ping += (unsigned short)i;

    if (i == iovec->siv_len && veclen == 1) {
      SU_DEBUG_7(("%s(%p): received %u bytes of keepalive\n",
		  __func__, (void *)self, (unsigned)i));
      msg_destroy(self->tp_msg), self->tp_msg = NULL;
      return 1;
    }
  }

  /* Write the received data to the message dump file */
  if (self->tp_master->mr_dump_file)
    tport_dump_iovec(self, msg, n, iovec, veclen, "recv", "from");
    
  if (self->tp_master->mr_capt_sock)
      tport_capt_msg(self, msg, n, iovec, veclen, "recv");
         

  /* Mark buffer as used */
  msg_recv_commit(msg, n, n == 0);

  if (n > 0)
    self->tp_ping = 0;

  return n != 0;
}

/** Send to stream */
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

/** Calculate timeout if receive is incomplete. */
int tport_next_recv_timeout(tport_t *self,
			    su_time_t *return_target,
			    char const **return_why)
{
  unsigned timeout = self->tp_params->tpp_timeout;

  if (timeout < INT_MAX) {
    /* Recv timeout */
    if (self->tp_msg) {
      su_time_t ntime = su_time_add(self->tp_rtime, timeout);
      if (su_time_cmp(ntime, *return_target) < 0)
	*return_target = ntime, *return_why = "recv timeout";
    }

#if 0
    /* Send timeout */
    if (tport_has_queued(self)) {
      su_time_t ntime = su_time_add(self->tp_stime, timeout);
      if (su_time_cmp(ntime, *return_target) < 0)
	*return_target = ntime, *return_why = "send timeout";
    }
#endif
  }

  return 0;
}

/** Timeout timer if receive is incomplete */
void tport_recv_timeout_timer(tport_t *self, su_time_t now)
{
  unsigned timeout = self->tp_params->tpp_timeout;

  if (timeout < INT_MAX) {
    if (self->tp_msg &&
	su_time_cmp(su_time_add(self->tp_rtime, timeout), now) < 0) {
      msg_t *msg = self->tp_msg;
      msg_set_streaming(msg, (enum msg_streaming_status)0);
      msg_set_flags(msg, MSG_FLG_ERROR | MSG_FLG_TRUNC | MSG_FLG_TIMEOUT);
      tport_deliver(self, msg, NULL, NULL, now);
      self->tp_msg = NULL;
    }

#if 0
    /* Send timeout */
    if (tport_has_queued(self) &&
	su_time_cmp(su_time_add(self->tp_stime, timeout), now) < 0) {
      stime = su_time_add(self->tp_stime, self->tp_params->tpp_timeout);
      if (su_time_cmp(stime, target) < 0)
	target = stime;
    }
#endif
  }
}

/** Calculate next timeout for keepalive */
int tport_next_keepalive(tport_t *self,
			 su_time_t *return_target,
			 char const **return_why)
{
  /* Keepalive timer */
  unsigned timeout = self->tp_params->tpp_keepalive;

  if (timeout != 0 && timeout != UINT_MAX) {
    if (!tport_has_queued(self)) {
      su_time_t ntime = su_time_add(self->tp_ktime, timeout);
      if (su_time_cmp(ntime, *return_target) < 0)
	*return_target = ntime, *return_why = "keepalive";
    }
  }

  timeout = self->tp_params->tpp_pingpong;
  if (timeout != 0) {
    if (self->tp_ptime.tv_sec && !self->tp_recv_close) {
      su_time_t ntime = su_time_add(self->tp_ptime, timeout);
      if (su_time_cmp(ntime, *return_target) < 0)
	*return_target = ntime, *return_why = "waiting for pong";
    }
  }

  return 0;
}


/** Keepalive timer. */
void tport_keepalive_timer(tport_t *self, su_time_t now)
{
  unsigned timeout = self->tp_params->tpp_pingpong;

  if (timeout != 0) {
    if (self->tp_ptime.tv_sec && !self->tp_recv_close &&
	su_time_cmp(su_time_add(self->tp_ptime, timeout), now) < 0) {
      SU_DEBUG_3(("%s(%p): %s to " TPN_FORMAT "%s\n",
		  __func__, (void *)self,
		  "closing connection", TPN_ARGS(self->tp_name),
		  " because of PONG timeout"));
      tport_error_report(self, EPIPE, NULL);
      if (!self->tp_closed)
	tport_close(self);
      return;
    }
  }

  timeout = self->tp_params->tpp_keepalive;

  if (timeout != 0 && timeout != UINT_MAX) {
    if (su_time_cmp(su_time_add(self->tp_ktime, timeout), now) < 0) {
      tport_tcp_ping(self, now);
    }
  }
}

/** Send PING */
int tport_tcp_ping(tport_t *self, su_time_t now)
{
  ssize_t n;
  char *why = "";

  if (tport_has_queued(self))
    return 0;

  n = send(self->tp_socket, "\r\n\r\n", 4, 0);

  if (n > 0)
    self->tp_ktime = now;

  if (n == 4) {
    if (self->tp_ptime.tv_sec == 0)
      self->tp_ptime = now;
  }
  else if (n == -1) {
    int error = su_errno();

    why = " failed";

    if (!su_is_blocking(error))
      tport_error_report(self, error, NULL);
    else
      why = " blocking";

    return -1;
  }

  SU_DEBUG_7(("%s(%p): %s to " TPN_FORMAT "%s\n",
	      __func__, (void *)self,
	      "sending PING", TPN_ARGS(self->tp_name), why));

  return n == -1 ? -1 : 0;
}

/** Send pong */
int tport_tcp_pong(tport_t *self)
{
  self->tp_ping = 0;

  if (tport_has_queued(self) || !self->tp_params->tpp_pong2ping)
    return 0;

  SU_DEBUG_7(("%s(%p): %s to " TPN_FORMAT "%s\n",
	      __func__, (void *)self,
	      "sending PONG", TPN_ARGS(self->tp_name), ""));

  return send(self->tp_socket, "\r\n", 2, 0);
}

/** Calculate next timer for TCP. */
int tport_tcp_next_timer(tport_t *self,
			 su_time_t *return_target,
			 char const **return_why)
{
  return
    tport_next_recv_timeout(self, return_target, return_why) |
    tport_next_keepalive(self, return_target, return_why);
}

/** TCP timer. */
void tport_tcp_timer(tport_t *self, su_time_t now)
{
  tport_recv_timeout_timer(self, now);
  tport_keepalive_timer(self, now);
  tport_base_timer(self, now);
}
