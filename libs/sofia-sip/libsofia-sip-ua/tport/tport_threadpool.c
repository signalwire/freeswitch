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

/**@CFILE tport_threadpool.c Multithreading transport
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Mar 24 08:45:49 EET 2006 ppessi
 */

#include "config.h"

#undef HAVE_SIGCOMP

#define SU_ROOT_MAGIC_T         struct tport_threadpool
#define SU_WAKEUP_ARG_T         struct tport_s
#define SU_MSG_ARG_T            union tport_su_msg_arg

#include "tport_internal.h"

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "tport_threadpool";
#endif

/* ==== Thread pools =================================================== */

typedef struct threadpool threadpool_t;

typedef struct {
  tport_primary_t tptp_primary;
  threadpool_t   *tptp_pool;   /**< Worker threads */
  unsigned        tptp_poolsize;
} tport_threadpool_t;

struct threadpool
{
  /* Shared */
  su_clone_r thrp_clone;
  tport_threadpool_t *thrp_tport;

  int        thrp_killing; /* Threadpool is being killed */

  /* Private variables */
  su_root_t    *thrp_root;
  int           thrp_reg;
  struct sigcomp_compartment *thrp_compartment;
  su_msg_r   thrp_rmsg;

  /* Slave thread counters */
  int        thrp_r_sent;
  int        thrp_s_recv;

  unsigned   thrp_rcvd_msgs;
  unsigned   thrp_rcvd_bytes;

  /* Master thread counters */
  int        thrp_s_sent;
  int        thrp_r_recv;

  int        thrp_yield;
};

typedef struct
{
  threadpool_t *tpd_thrp;
  int  tpd_errorcode;
  msg_t *tpd_msg;
  su_time_t tpd_when;
  unsigned tpd_mtu;
#if HAVE_SIGCOMP
  struct sigcomp_compartment *tpd_cc;
#endif
  struct sigcomp_udvm *tpd_udvm;
  socklen_t tpd_namelen;
  su_sockaddr_t tpd_name[1];
} thrp_udp_deliver_t;

union tport_su_msg_arg
{
  threadpool_t   *thrp;
  thrp_udp_deliver_t thrp_udp_deliver[1];
};

int tport_threadpool_init_primary(tport_primary_t *,
				  tp_name_t tpn[1],
				  su_addrinfo_t *,
				  tagi_t const *,
				  char const **return_culprit);
static void tport_threadpool_deinit_primary(tport_primary_t *pri);

static int tport_thread_send(tport_t *tp,
			     msg_t *msg,
			     tp_name_t const *tpn,
			     struct sigcomp_compartment *cc,
			     unsigned mtu);

tport_vtable_t const tport_threadpool_vtable =
{
  /* vtp_name 		     */ "udp",
  /* vtp_public              */ tport_type_local,
  /* vtp_pri_size            */ sizeof (tport_threadpool_t),
  /* vtp_init_primary        */ tport_threadpool_init_primary,
  /* vtp_deinit_primary      */ tport_threadpool_deinit_primary,
  /* vtp_wakeup_pri          */ NULL,
  /* vtp_connect             */ NULL,
  /* vtp_secondary_size      */ 0, /* No secondary transports! */
  /* vtp_init_secondary      */ NULL,
  /* vtp_deinit_secondary    */ NULL,
  /* vtp_shutdown            */ NULL,
  /* vtp_set_events          */ NULL,
  /* vtp_wakeup              */ NULL,
  /* vtp_recv                */ tport_recv_dgram,
  /* vtp_send                */ tport_send_dgram,
  /* vtp_deliver             */ NULL,
  /* vtp_prepare             */ tport_thread_send,
  /* vtp_keepalive           */ NULL,
  /* vtp_stun_response       */ NULL,
  /* vtp_next_secondary_timer*/ NULL,
  /* vtp_secondary_timer     */ NULL,
};

static int thrp_udp_init(su_root_t *, threadpool_t *);
static void thrp_udp_deinit(su_root_t *, threadpool_t *);
static int thrp_udp_event(threadpool_t *thrp,
			    su_wait_t *w,
			    tport_t *_tp);
static int thrp_udp_recv_deliver(threadpool_t *thrp,
				 tport_t const *tp,
				 thrp_udp_deliver_t *tpd,
				 int events);
static int thrp_udp_recv(threadpool_t *thrp, thrp_udp_deliver_t *tpd);
#if HAVE_SIGCOMP
static int thrp_udvm_decompress(threadpool_t *thrp,
				thrp_udp_deliver_t *tpd);
#endif
static void thrp_udp_deliver(threadpool_t *thrp,
			     su_msg_r msg,
			     union tport_su_msg_arg *arg);
static void thrp_udp_deliver_report(threadpool_t *thrp,
				    su_msg_r m,
				    union tport_su_msg_arg *arg);
static void thrp_udp_send(threadpool_t *thrp,
			  su_msg_r msg,
			  union tport_su_msg_arg *arg);
static void thrp_udp_send_report(threadpool_t *thrp,
				 su_msg_r msg,
				 union tport_su_msg_arg *arg);


/** Launch threads in the tport pool. */
int tport_threadpool_init_primary(tport_primary_t *pri,
				  tp_name_t tpn[1],
				  su_addrinfo_t *ai,
				  tagi_t const *tags,
				  char const **return_culprit)
{
  tport_threadpool_t *tptp = (tport_threadpool_t *)pri;
  tport_t *tp = pri->pri_primary;
  threadpool_t *thrp;
  int i, N = tp->tp_params->tpp_thrpsize;

  assert(ai->ai_socktype == SOCK_DGRAM);

  if (tport_udp_init_primary(pri, tpn, ai, tags, return_culprit) < 0)
    return -1;

  if (N == 0)
    return 0;

  thrp = su_zalloc(tp->tp_home, (sizeof *thrp) * N);
  if (!thrp)
    return -1;

  su_setblocking(tp->tp_socket, 0);

  tptp->tptp_pool = thrp;
  tptp->tptp_poolsize = N;

  for (i = 0; i < N; i++) {
#if HAVE_SIGCOMP
    if (tport_has_sigcomp(tp))
      thrp[i].thrp_compartment = tport_primary_compartment(tp->tp_master);
#endif
    thrp[i].thrp_tport = tptp;
    if (su_clone_start(pri->pri_master->mr_root,
		       thrp[i].thrp_clone,
		       thrp + i,
		       thrp_udp_init,
		       thrp_udp_deinit) < 0)
      goto error;
  }

  tp->tp_events = 0;

  return 0;

 error:
  assert(!"tport_launch_threadpool");
  return -1;
}

/** Kill threads in the tport pool.
 *
 * @note Executed by stack thread only.
 */
static
void tport_threadpool_deinit_primary(tport_primary_t *pri)
{
  tport_threadpool_t *tptp = (tport_threadpool_t *)pri;
  threadpool_t *thrp = tptp->tptp_pool;
  int i, N = pri->tptp_poolsize;

  if (!thrp)
    return;

  /* Prevent application from using these. */
  for (i = 0; i < N; i++)
    thrp[i].thrp_killing = 1;

  /* Stop every task in the threadpool. */
  for (i = 0; i < N; i++)
    su_clone_wait(pri->pri_master->mr_root, thrp[i].thrp_clone);

  su_free(pri->pri_home, tptp), tptp->tptp_pool = NULL;
  tptp->tptp_poolsize = 0;

  SU_DEBUG_3(("%s(%p): zapped threadpool\n", __func__, pri));
}

static int thrp_udp_init(su_root_t *root, threadpool_t *thrp)
{
  tport_t *tp = thrp->thrp_tport->tptp_primary->pri_primary;
  su_wait_t wait[1];

  assert(tp);

  thrp->thrp_root = root;

  if (su_wait_create(wait, tp->tp_socket, SU_WAIT_IN | SU_WAIT_ERR) < 0)
    return -1;

  thrp->thrp_reg = su_root_register(root, wait, thrp_udp_event, tp, 0);

  if (thrp->thrp_reg  == -1)
    return -1;

  return 0;
}

static void thrp_udp_deinit(su_root_t *root, threadpool_t *thrp)
{
  if (thrp->thrp_reg)
    su_root_deregister(root, thrp->thrp_reg), thrp->thrp_reg = 0;
  su_msg_destroy(thrp->thrp_rmsg);
}

su_inline void
thrp_yield(threadpool_t *thrp)
{
  tport_t *tp = thrp->thrp_tport->tptp_primary->pri_primary;
  su_root_eventmask(thrp->thrp_root, thrp->thrp_reg, tp->tp_socket, 0);
  thrp->thrp_yield = 1;
}

su_inline void
thrp_gain(threadpool_t *thrp)
{
  tport_t *tp = thrp->thrp_tport->tptp_primary->pri_primary;
  int events = SU_WAIT_IN | SU_WAIT_ERR;
  su_root_eventmask(thrp->thrp_root, thrp->thrp_reg, tp->tp_socket, events);
  thrp->thrp_yield = 0;
}

static int thrp_udp_event(threadpool_t *thrp,
			  su_wait_t *w,
			  tport_t *tp)
{
#if HAVE_POLL
  assert(w->fd == tp->tp_socket);
#endif

  for (;;) {
    thrp_udp_deliver_t *tpd;
    int events;

    if (!*thrp->thrp_rmsg) {
      if (su_msg_create(thrp->thrp_rmsg,
			su_root_parent(thrp->thrp_root),
			su_root_task(thrp->thrp_root),
			thrp_udp_deliver,
			sizeof (*tpd)) == -1) {
	SU_DEBUG_1(("thrp_udp_event(%p): su_msg_create(): %s\n", thrp,
		    strerror(errno)));
	return 0;
      }
    }

    tpd = su_msg_data(thrp->thrp_rmsg)->thrp_udp_deliver; assert(tpd);
    tpd->tpd_thrp = thrp;

    events = su_wait_events(w, tp->tp_socket);
    if (!events)
      return 0;

    thrp_udp_recv_deliver(thrp, tp, tpd, events);

    if (*thrp->thrp_rmsg) {
      SU_DEBUG_7(("thrp_udp_event(%p): no msg sent\n", thrp));
      tpd = su_msg_data(thrp->thrp_rmsg)->thrp_udp_deliver;
      memset(tpd, 0, sizeof *tpd);
      return 0;
    }

    if (thrp->thrp_yield || (thrp->thrp_s_sent - thrp->thrp_s_recv) > 0)
      return 0;

    su_wait(w, 1, 0);
  }
}

static int thrp_udp_recv_deliver(threadpool_t *thrp,
				 tport_t const *tp,
				 thrp_udp_deliver_t *tpd,
				 int events)
{
  unsigned qlen = thrp->thrp_r_sent - thrp->thrp_r_recv;

  SU_DEBUG_7(("thrp_udp_event(%p): events%s%s%s%s for %p\n", thrp,
	      events & SU_WAIT_IN ? " IN" : "",
	      events & SU_WAIT_HUP ? " HUP" : "",
	      events & SU_WAIT_OUT ? " OUT" : "",
	      events & SU_WAIT_ERR ? " ERR" : "",
	      tpd));

  if (events & SU_WAIT_ERR) {
    tpd->tpd_errorcode = tport_udp_error(tp, tpd->tpd_name);
    if (tpd->tpd_errorcode) {
      if (thrp->thrp_yield)
	su_msg_report(thrp->thrp_rmsg, thrp_udp_deliver_report);
      tpd->tpd_when = su_now();
      su_msg_send(thrp->thrp_rmsg);
      thrp->thrp_r_sent++;
      return 0;
    }
  }

  if (events & SU_WAIT_IN) {
    if (thrp_udp_recv(thrp, tpd) < 0) {
      tpd->tpd_errorcode = su_errno();
      assert(tpd->tpd_errorcode);
      if (su_is_blocking(tpd->tpd_errorcode))
	return 0;
    }
    else if (tpd->tpd_msg) {
      int n = msg_extract(tpd->tpd_msg); (void)n;

      thrp->thrp_rcvd_msgs++;
      thrp->thrp_rcvd_bytes += msg_size(tpd->tpd_msg);
    }

#if HAVE_SIGCOMP
    if (tpd->tpd_udvm && !tpd->tpd_msg)
      sigcomp_udvm_free(tpd->tpd_udvm), tpd->tpd_udvm = NULL;
#endif

    assert(!tpd->tpd_msg || !tpd->tpd_errorcode);

    if (tpd->tpd_msg || tpd->tpd_errorcode) {
      if (qlen >= tp->tp_params->tpp_thrprqsize) {
	SU_DEBUG_7(("tport recv queue %i: %u\n",
		    (int)(thrp - tp->tp_pri->tptp_pool), qlen));
	thrp_yield(thrp);
      }

      if (qlen >= tp->tp_params->tpp_thrprqsize / 2)
	su_msg_report(thrp->thrp_rmsg, thrp_udp_deliver_report);
      tpd->tpd_when = su_now();
      su_msg_send(thrp->thrp_rmsg);
      thrp->thrp_r_sent++;
      return 0;
    }
  }

  return 0;
}

#include <pthread.h>

/** Mutex for reading from socket */
static pthread_mutex_t mutex[1] = { PTHREAD_MUTEX_INITIALIZER };

/** Receive a UDP packet by threadpool. */
static
int thrp_udp_recv(threadpool_t *thrp, thrp_udp_deliver_t *tpd)
{
  tport_t const *tp = thrp->thrp_tport->pri_primary;
  unsigned char sample[2];
  int N;
  int s = tp->tp_socket;

  pthread_mutex_lock(mutex);

  /* Simulate packet loss */
  if (tp->tp_params->tpp_drop &&
      su_randint(0, 1000) < tp->tp_params->tpp_drop) {
    recv(s, sample, 1, 0);
    pthread_mutex_unlock(mutex);
    SU_DEBUG_3(("tport(%p): simulated packet loss!\n", tp));
    return 0;
  }

  /* Peek for first two bytes in message:
     determine if this is stun, sigcomp or sip
  */
  N = recv(s, sample, sizeof sample, MSG_PEEK | MSG_TRUNC);

  if (N < 0) {
    if (su_is_blocking(su_errno()))
      N = 0;
  }
  else if (N <= 1) {
    SU_DEBUG_1(("%s(%p): runt of %u bytes\n", "thrp_udp_recv", thrp, N));
    recv(s, sample, sizeof sample, 0);
    N = 0;
  }
#if !HAVE_MSG_TRUNC
  else if ((N = su_getmsgsize(tp->tp_socket)) < 0)
    ;
#endif
  else if ((sample[0] & 0xf8) == 0xf8) {
#if HAVE_SIGCOMP
    if (thrp->thrp_compartment) {
      struct sigcomp_buffer *input;
      void *data;
      int dlen;

      tpd->tpd_udvm =
	sigcomp_udvm_create_for_compartment(thrp->thrp_compartment);
      input = sigcomp_udvm_input_buffer(tpd->tpd_udvm, N); assert(input);

      data = input->b_data + input->b_avail;
      dlen = input->b_size - input->b_avail;

      if (dlen < N)
	dlen = 0;

      tpd->tpd_namelen = sizeof(tpd->tpd_name);

      dlen = recvfrom(tp->tp_socket, data, dlen, 0,
		      &tpd->tpd_name->su_sa, &tpd->tpd_namelen);

      SU_CANONIZE_SOCKADDR(tpd->tpd_name);

      if (dlen < N) {
	su_seterrno(EMSGSIZE);		/* Protocol error */
	N = -1;
      } else if (dlen == -1)
	N = -1;
      else {
	input->b_avail += dlen;
	input->b_complete = 1;

	pthread_mutex_unlock(mutex);

	N = thrp_udvm_decompress(thrp, tpd);

	if (N == -1)
	  /* Do not report decompression errors as ICMP errors */
	  memset(tpd->tpd_name, 0, tpd->tpd_namelen);

	return N;
      }
      pthread_mutex_unlock(mutex);
      return N;
    }
#endif
    recv(s, sample, 1, 0);
    pthread_mutex_unlock(mutex);
    /* XXX - send NACK ? */
    su_seterrno(EBADMSG);
    N = -1;
  }
  else {
    /* receive as usual */
    N = tport_recv_dgram_r(tp, &tpd->tpd_msg, N);
  }

  pthread_mutex_unlock(mutex);

  return N;
}

#if HAVE_SIGCOMP
static
int thrp_udvm_decompress(threadpool_t *thrp, thrp_udp_deliver_t *tpd)
{
  struct sigcomp_udvm *udvm = tpd->tpd_udvm;
  struct sigcomp_buffer *output;
  msg_iovec_t iovec[msg_n_fragments] = {{ 0 }};
  su_addrinfo_t *ai;
  tport_t *tp = thrp->thrp_tport->pri_primary;
  size_t n, m, i, dlen;
  int eos;
  void *data;
  ssize_t veclen;

  output = sigcomp_udvm_output_buffer(udvm, -1);

  if (sigcomp_udvm_decompress(udvm, output, NULL) < 0) {
    int error = sigcomp_udvm_errno(udvm);
    SU_DEBUG_3(("%s: UDVM error %d: %s\n", __func__,
		error, sigcomp_udvm_strerror(udvm)));
    su_seterrno(EREMOTEIO);
    return -1;
  }

  data = output->b_data + output->b_used;
  dlen = output->b_avail - output->b_used;
  /* XXX - if a message is larger than default output size... */
  eos = output->b_complete; assert(output->b_complete);

  veclen = tport_recv_iovec(tp, &tpd->tpd_msg, iovec, dlen, eos);

  if (veclen <= 0) {
    n = -1;
  } else {
    for (i = 0, n = 0; i < veclen; i++) {
      m = iovec[i].mv_len; assert(dlen >= n + m);
      memcpy(iovec[i].mv_base, data + n, m);
      n += m;
    }
    assert(dlen == n);

    msg_recv_commit(tpd->tpd_msg, dlen, eos);    /* Mark buffer as used */

    /* Message address */
    ai = msg_addrinfo(tpd->tpd_msg);
    ai->ai_flags |= TP_AI_COMPRESSED;
    ai->ai_family = tpd->tpd_name->su_sa.sa_family;
    ai->ai_socktype = SOCK_DGRAM;
    ai->ai_protocol = IPPROTO_UDP;
    memcpy(ai->ai_addr, tpd->tpd_name, ai->ai_addrlen = tpd->tpd_namelen);

    SU_DEBUG_9(("%s(%p): sigcomp msg sz = %d\n", __func__, tp, n));
  }

  return n;
}
#endif

/** Deliver message from threadpool to the stack
 *
 * @note Executed by stack thread only.
 */
static
void thrp_udp_deliver(su_root_magic_t *magic,
		      su_msg_r m,
		      union tport_su_msg_arg *arg)
{
  thrp_udp_deliver_t *tpd = arg->thrp_udp_deliver;
  threadpool_t *thrp = tpd->tpd_thrp;
  tport_t *tp = thrp->thrp_tport->pri_primary;
  su_time_t now = su_now();

  assert(magic != thrp);

  thrp->thrp_r_recv++;

  if (thrp->thrp_killing) {
#if HAVE_SIGCOMP
    sigcomp_udvm_free(tpd->tpd_udvm), tpd->tpd_udvm = NULL;
#endif
    msg_destroy(tpd->tpd_msg);
    return;
  }

  SU_DEBUG_7(("thrp_udp_deliver(%p): got %p delay %f\n",
	      thrp, tpd, 1000 * su_time_diff(now, tpd->tpd_when)));

  if (tpd->tpd_errorcode)
    tport_error_report(tp, tpd->tpd_errorcode, tpd->tpd_name);
  else if (tpd->tpd_msg) {
    tport_deliver(tp, tpd->tpd_msg, NULL, &tpd->tpd_udvm, tpd->tpd_when);
    tp->tp_rlogged = NULL;
  }

#if HAVE_SIGCOMP
  if (tpd->tpd_udvm) {
    sigcomp_udvm_free(tpd->tpd_udvm), tpd->tpd_udvm = NULL;
  }
#endif
}

static
void thrp_udp_deliver_report(threadpool_t *thrp,
			     su_msg_r m,
			     union tport_su_msg_arg *arg)
{
  if (thrp->thrp_yield) {
    int qlen = thrp->thrp_r_sent - thrp->thrp_r_recv;
    int qsize = thrp->thrp_tport->pri_params->tpp_thrprqsize;
    if (qlen == 0 || qlen < qsize / 2)
      thrp_gain(thrp);
  }
}

/** Send a message to network using threadpool.
 *
 * @note Executed by stack thread only.
 */
static
int tport_thread_send(tport_t *tp,
		      msg_t *msg,
		      tp_name_t const *tpn,
		      struct sigcomp_compartment *cc,
		      unsigned mtu)
{

  threadpool_t *thrp = tp->tp_pri->tptp_pool;
  thrp_udp_deliver_t *tpd;
  int i, N = tp->tp_pri->tptp_poolsize;
  su_msg_r m;
  unsigned totalqlen = 0;
  unsigned qlen;

  if (!tp->tp_pri->tptp_pool)
    return tport_prepare_and_send(tp, msg, tpn, cc, mtu);

  SU_DEBUG_9(("tport_thread_send()\n"));

  if (thrp->thrp_killing)
    return (su_seterrno(ECHILD)), -1;

  qlen = totalqlen = thrp->thrp_s_sent - thrp->thrp_s_recv;

  /* Select thread with shortest queue */
  for (i = 1; i < N; i++) {
    threadpool_t *other = tp->tp_pri->tptp_pool + i;
    unsigned len = other->thrp_s_sent - other->thrp_s_recv;

    if (len < qlen ||
	(len == qlen && (other->thrp_s_sent - thrp->thrp_s_sent) < 0))
      thrp = other, qlen = len;

    totalqlen += len;
  }

  if (totalqlen >= N * tp->tp_params->tpp_qsize)
    SU_DEBUG_3(("tport send queue: %u (shortest %u)\n", totalqlen, qlen));

  if (su_msg_create(m,
		    su_clone_task(thrp->thrp_clone),
		    su_root_task(tp->tp_master->mr_root),
		    thrp_udp_send,
		    sizeof (*tpd)) != su_success) {
    SU_DEBUG_1(("thrp_udp_event(%p): su_msg_create(): %s\n", thrp,
		strerror(errno)));
    return -1;
  }

  tpd = su_msg_data(m)->thrp_udp_deliver;
  tpd->tpd_thrp = thrp;
  tpd->tpd_when = su_now();
  tpd->tpd_mtu = mtu;
  tpd->tpd_msg = msg_ref_create(msg);

#if HAVE_SIGCOMP
  tpd->tpd_cc = cc;
#endif

  su_msg_report(m, thrp_udp_send_report);

  if (su_msg_send(m) == su_success) {
    thrp->thrp_s_sent++;
    return 0;
  }

  msg_ref_destroy(msg);
  return -1;
}

/** thrp_udp_send() is run by threadpool to send the message. */
static
void thrp_udp_send(threadpool_t *thrp,
		   su_msg_r m,
		   union tport_su_msg_arg *arg)
{
  thrp_udp_deliver_t *tpd = arg->thrp_udp_deliver;
  tport_t *tp = thrp->thrp_tport->pri_primary;
  msg_t *msg = tpd->tpd_msg;
  msg_iovec_t *iov, auto_iov[40], *iov0 = NULL;
  int iovlen, iovused, n;

  assert(thrp == tpd->tpd_thrp);

  thrp->thrp_s_recv++;

  {
    double delay = 1000 * su_time_diff(su_now(), tpd->tpd_when);
    if (delay > 100)
      SU_DEBUG_3(("thrp_udp_deliver(%p): got %p delay %f\n", thrp, tpd, delay));
    else
      SU_DEBUG_7(("thrp_udp_deliver(%p): got %p delay %f\n", thrp, tpd, delay));
  }

  if (!msg) {
    tpd->tpd_errorcode = EINVAL;
    return;
  }

  /* Prepare message for sending - i.e., encode it */
  if (msg_prepare(msg) < 0) {
    tpd->tpd_errorcode = errno;
    return;
  }

  if (tpd->tpd_mtu != 0 && msg_size(msg) > tpd->tpd_mtu) {
    tpd->tpd_errorcode = EMSGSIZE;
    return;
  }

  /* Use initially the I/O vector from stack */
  iov = auto_iov, iovlen = sizeof(auto_iov)/sizeof(auto_iov[0]);

  /* Get a iovec for message contents */
  for (;;) {
    iovused = msg_iovec(msg, iov, iovlen);
    if (iovused <= iovlen)
      break;

    iov = iov0 = realloc(iov0, sizeof(*iov) * iovused);
    iovlen = iovused;

    if (iov0 == NULL) {
      tpd->tpd_errorcode = errno;
      return;
    }
  }

  assert(iovused > 0);

  tpd->tpd_when = su_now();

  if (0)
    ;
#if HAVE_SIGCOMP
  else if (tpd->tpd_cc) {
    tport_sigcomp_t sc[1] = {{ NULL }};

    n = tport_sigcomp_vsend(tp, msg, iov, iovused, tpd->tpd_cc, sc);
  }
#endif
  else
    n = tport_send_dgram(tp, msg, iov, iovused);

  if (n == -1)
    tpd->tpd_errorcode = su_errno();

  if (iov0)
    free(iov0);
}

static
void thrp_udp_send_report(su_root_magic_t *magic,
			  su_msg_r msg,
			  union tport_su_msg_arg *arg)
{
  thrp_udp_deliver_t *tpd = arg->thrp_udp_deliver;
  threadpool_t *thrp = tpd->tpd_thrp;
  tport_t *tp = thrp->thrp_tport->pri_primary;

  assert(magic != thrp);

  SU_DEBUG_7(("thrp_udp_send_report(%p): got %p delay %f\n",
	      thrp, tpd, 1000 * su_time_diff(su_now(), tpd->tpd_when)));

  if (tp->tp_master->mr_log)
    tport_log_msg(tp, tpd->tpd_msg, "sent", "to", tpd->tpd_when);

  if (tpd->tpd_errorcode)
    tport_error_report(tp, tpd->tpd_errorcode, msg_addr(tpd->tpd_msg));

  msg_ref_destroy(tpd->tpd_msg);

}
