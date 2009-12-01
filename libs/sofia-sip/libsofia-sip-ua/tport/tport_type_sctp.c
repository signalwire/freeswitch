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

/**@CFILE tport_type_sctp.c Transport using SCTP.
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @RFC4168.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Mar 24 08:45:49 EET 2006 ppessi
 * @date Original Created: Thu Jul 20 12:54:32 2000 ppessi
 */

#include "config.h"

#if HAVE_SCTP

#include "tport_internal.h"

#if HAVE_NETINET_SCTP_H
#include <netinet/sctp.h>
#endif

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>

/* ---------------------------------------------------------------------- */
/* SCTP */

#undef MAX_STREAMS
#define MAX_STREAMS MAX_STREAMS

/* Missing socket symbols */
#ifndef SOL_SCTP
#define SOL_SCTP IPPROTO_SCTP
#endif

enum { MAX_STREAMS = 1 };
typedef struct tport_sctp_t
{
  tport_t sctp_base[1];

  msg_t *sctp_recv[MAX_STREAMS];
  struct sctp_send {
    msg_t *ss_msg;
    msg_iovec_t *ss_unsent;	/**< Pointer to first unsent iovec */
    unsigned     ss_unsentlen;  /**< Number of unsent iovecs */
    msg_iovec_t *ss_iov;	/**< Iovecs allocated for sending */
    unsigned     ss_iovlen;	/**< Number of allocated iovecs */
  } sctp_send[MAX_STREAMS];
} tport_sctp_t;

#define TP_SCTP_MSG_MAX (65536)

static int tport_sctp_init_primary(tport_primary_t *,
				   tp_name_t tpn[1],
				   su_addrinfo_t *, tagi_t const *,
				   char const **return_culprit);
static int tport_sctp_init_client(tport_primary_t *,
				  tp_name_t tpn[1],
				  su_addrinfo_t *, tagi_t const *,
				  char const **return_culprit);
static int tport_sctp_init_secondary(tport_t *self, int socket, int accepted,
				     char const **return_reason);
static int tport_sctp_init_socket(tport_primary_t *pri,
				  int socket,
				  char const **return_reason);
static int tport_recv_sctp(tport_t *self);
static ssize_t tport_send_sctp(tport_t const *self, msg_t *msg,
			       msg_iovec_t iov[], size_t iovused);

static int tport_sctp_next_timer(tport_t *self, su_time_t *, char const **);
static void tport_sctp_timer(tport_t *self, su_time_t);

tport_vtable_t const tport_sctp_client_vtable =
{
  /* vtp_name 		     */ "sctp",
  /* vtp_public              */ tport_type_client,
  /* vtp_pri_size            */ sizeof (tport_primary_t),
  /* vtp_init_primary        */ tport_sctp_init_client,
  /* vtp_deinit_primary      */ NULL,
  /* vtp_wakeup_pri          */ tport_accept,
  /* vtp_connect             */ NULL,
  /* vtp_secondary_size      */ sizeof (tport_t),
  /* vtp_init_secondary      */ tport_sctp_init_secondary,
  /* vtp_deinit_secondary    */ NULL,
  /* vtp_shutdown            */ NULL,
  /* vtp_set_events          */ NULL,
  /* vtp_wakeup              */ NULL,
  /* vtp_recv                */ tport_recv_sctp,
  /* vtp_send                */ tport_send_sctp,
  /* vtp_deliver             */ NULL,
  /* vtp_prepare             */ NULL,
  /* vtp_keepalive           */ NULL,
  /* vtp_stun_response       */ NULL,
  /* vtp_next_secondary_timer*/ tport_sctp_next_timer,
  /* vtp_secondary_timer     */ tport_sctp_timer,
};

tport_vtable_t const tport_sctp_vtable =
{
  /* vtp_name 		     */ "sctp",
  /* vtp_public              */ tport_type_local,
  /* vtp_pri_size            */ sizeof (tport_primary_t),
  /* vtp_init_primary        */ tport_sctp_init_primary,
  /* vtp_deinit_primary      */ NULL,
  /* vtp_wakeup_pri          */ tport_accept,
  /* vtp_connect             */ NULL,
  /* vtp_secondary_size      */ sizeof (tport_t),
  /* vtp_init_secondary      */ tport_sctp_init_secondary,
  /* vtp_deinit_secondary    */ NULL,
  /* vtp_shutdown            */ NULL,
  /* vtp_set_events          */ NULL,
  /* vtp_wakeup              */ NULL,
  /* vtp_recv                */ tport_recv_sctp,
  /* vtp_send                */ tport_send_sctp,
  /* vtp_deliver             */ NULL,
  /* vtp_prepare             */ NULL,
  /* vtp_keepalive           */ NULL,
  /* vtp_stun_response       */ NULL,
  /* vtp_next_secondary_timer*/ tport_sctp_next_timer,
  /* vtp_secondary_timer     */ tport_sctp_timer,
};

static int tport_sctp_init_primary(tport_primary_t *pri,
				   tp_name_t tpn[1],
				   su_addrinfo_t *ai,
				   tagi_t const *tags,
				   char const **return_culprit)
{
  int socket;

  if (pri->pri_params->tpp_mtu > TP_SCTP_MSG_MAX)
    pri->pri_params->tpp_mtu = TP_SCTP_MSG_MAX;

  socket = su_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);

  if (socket == INVALID_SOCKET)
    return *return_culprit = "socket", -1;

  if (tport_sctp_init_socket(pri, socket, return_culprit) < 0)
    return -1;

  return tport_stream_init_primary(pri, socket, tpn, ai, tags, return_culprit);
}

static int tport_sctp_init_client(tport_primary_t *pri,
				  tp_name_t tpn[1],
				  su_addrinfo_t *ai,
				  tagi_t const *tags,
				  char const **return_culprit)
{
  if (pri->pri_params->tpp_mtu > TP_SCTP_MSG_MAX)
    pri->pri_params->tpp_mtu = TP_SCTP_MSG_MAX;

  return tport_tcp_init_client(pri, tpn, ai, tags, return_culprit);
}

static int tport_sctp_init_secondary(tport_t *self, int socket, int accepted,
				     char const **return_reason)
{
  self->tp_has_connection = 1;

  if (accepted) {
    /* Accepted socket inherit the init information from listen socket */
    return 0;
  }
  else {
    return tport_sctp_init_socket(self->tp_pri, socket, return_reason);
  }
}

/** Initialize a SCTP socket */
static int tport_sctp_init_socket(tport_primary_t *pri,
				  int socket,
				  char const **return_reason)
{
  struct sctp_initmsg initmsg = { 0 };

  initmsg.sinit_num_ostreams = MAX_STREAMS;
  initmsg.sinit_max_instreams = MAX_STREAMS;

  if (setsockopt(socket, SOL_SCTP, SCTP_INITMSG, &initmsg, sizeof initmsg) < 0)
    return *return_reason = "SCTP_INITMSG", -1;

  return 0;
}

/** Receive data available on the socket.
 *
 * @retval -1 error
 * @retval 0  end-of-stream
 * @retval 1  normal receive
 * @retval 2  incomplete recv, recv again
 */
static
int tport_recv_sctp(tport_t *self)
{
  msg_t *msg;
  ssize_t N, veclen;
  msg_iovec_t iovec[2] = {{ 0 }};

  char sctp_buf[TP_SCTP_MSG_MAX];

  iovec[0].mv_base = sctp_buf;
  iovec[0].mv_len = sizeof(sctp_buf);

  N = su_vrecv(self->tp_socket, iovec, 1, 0, NULL, NULL);
  if (N == SOCKET_ERROR) {
    return su_is_blocking(su_errno()) ? 1 : -1;
  }

  if (N == 0) {
    if (self->tp_msg)
      msg_recv_commit(self->tp_msg, 0, 1);
    return 0;    /* End of stream */
  }

  tport_recv_bytes(self, N, N);

  veclen = tport_recv_iovec(self, &self->tp_msg, iovec, N, 0);
  if (veclen < 0)
    return -1;

  assert(veclen == 1); assert(iovec[0].mv_len == N);
  msg = self->tp_msg;

  msg_set_address(msg, self->tp_addr, self->tp_addrlen);

  memcpy(iovec[0].mv_base, sctp_buf, iovec[0].mv_len);

  if (self->tp_master->mr_dump_file)
    tport_dump_iovec(self, msg, N, iovec, veclen, "recv", "from");

  msg_recv_commit(msg, N, 0);  /* Mark buffer as used */

  return 2;
}

static ssize_t tport_send_sctp(tport_t const *self, msg_t *msg,
			       msg_iovec_t iov[], size_t iovused)
{


  return su_vsend(self->tp_socket, iov, iovused, MSG_NOSIGNAL, NULL, 0);
}

/** Calculate tick timer if send is pending. */
int tport_next_sctp_send_tick(tport_t *self,
			    su_time_t *return_target,
			    char const **return_why)
{
  unsigned timeout = 100;  /* Retry 10 times a second... */

  if (tport_has_queued(self)) {
    su_time_t ntime = su_time_add(self->tp_ktime, timeout);
    if (su_time_cmp(ntime, *return_target) < 0)
      *return_target = ntime, *return_why = "send tick";
  }

  return 0;
}

/** Tick timer if send is pending */
void tport_sctp_send_tick_timer(tport_t *self, su_time_t now)
{
  unsigned timeout = 100;

  /* Send timeout */
  if (tport_has_queued(self) &&
      su_time_cmp(su_time_add(self->tp_ktime, timeout), now) < 0) {
    uint64_t bytes = self->tp_stats.sent_bytes;
    su_time_t stime = self->tp_stime;

    tport_send_queue(self);

    if (self->tp_stats.sent_bytes == bytes)
      self->tp_stime = stime;	/* Restore send timestamp */
  }
}

/** Calculate next timer for SCTP. */
int tport_sctp_next_timer(tport_t *self,
			 su_time_t *return_target,
			 char const **return_why)
{
  return
    tport_next_recv_timeout(self, return_target, return_why) |
    tport_next_sctp_send_tick(self, return_target, return_why);
}

/** SCTP timer. */
void tport_sctp_timer(tport_t *self, su_time_t now)
{
  tport_sctp_send_tick_timer(self, now);
  tport_recv_timeout_timer(self, now);
  tport_base_timer(self, now);
}

#else
/* ISO c99 forbids empty source file */
void *sofia_tport_type_sctp_dummy;
#endif
