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

/**@CFILE tport_sigcomp.c Transport using SigComp.
 *
 * Incomplete.
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Fri Mar 24 08:45:49 EET 2006 ppessi
 */

#include "config.h"

#include "tport.h"

#include <sofia-sip/su_string.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>

#include <sigcomp.h>

/* ---------------------------------------------------------------------- */
/* SigComp */

typedef struct tport_sigcomp_handler tport_sigcomp_handler_t;

/** @internal Per end-point SigComp data */
struct tport_compressor {
  struct sigcomp_state_handler *msc_state_handler;
  struct sigcomp_compartment *msc_compartment;
};

/** @internal Per-socket SigComp data */
struct tport_comp {
  struct sigcomp_udvm          *sc_udvm;
  struct sigcomp_compartment   *sc_cc;
  struct sigcomp_compressor    *sc_compressor;
  struct sigcomp_buffer        *sc_output;
  unsigned                      sc_compressed;

  struct sigcomp_buffer        *sc_input;
  unsigned                      sc_copied;

  enum {
    format_is_unknown,
    format_is_sigcomp,
    format_is_noncomp
  } sc_infmt, sc_outfmt;
};


tport_compressor_t *vsc_master_init_sigcomp(tport_t *mr,
					     char const *algorithm)
{
  tport_compressor_t *retval =  NLL;
  stuct sigcomp_state_handler *sh = NULL;
  struct sigcomp_algorithm *a = NULL;
  struct sigcomp_compartment *cc = NULL;

  if (algorithm == NULL)
    algorithm = getenv("SIGCOMP_ALGORITHM");

  retval = su_zalloc((su_home_t *)mr, sizeof *retval);
  if (retval == NULL)
    return retval;

  sh = sigcomp_state_handler_create();

  if (sh == NULL) {
    SU_DEBUG_1(("tport: initializing SigComp state handler: %s\n",
		strerror(errno)));
    vsc_master_deinit_sigcomp(mr, retval), retval = NULL;
    return retval;
  }
  retval->msc_state_handler = sh;

  a = sigcomp_algorithm_by_name(algorithm);
  cc = sigcomp_compartment_create(a, sh, 0, "", 0, NULL, 0);

  if (cc == NULL) {
    SU_DEBUG_1(("tport: initializing SigComp master compartment: %s\n",
		strerror(errno)));
    vsc_master_deinit_sigcomp(mr, retval), retval = NULL;
    return retval;
  }

  self->msc_compartment = cc;

  return retval;
}

  if (self->sa_compartment) {
    agent_sigcomp_options(self, self->sa_compartment);
    sigcomp_compartment_option(self->sa_compartment, "stateless");
  }
  else

  if (mr->mr_compartment)
    sigcomp_compartment_unref(mt->mr_compartment);
  mr->mr_compartment = sigcomp_compartment_ref(cc);

  return cc;
}


void vsc_master_deinit_sigcomp(tport_master_t *mr)
{
  tport_sigcomp_vtable_t const *vsc = tport_sigcomp_vtable;

  if (mr->mr_compartment)
    sigcomp_compartment_unref(mr->mr_compartment), mr->mr_compartment = NULL;

}

char const *vsc_comp_name(tport_sigcomp_t const *master_sc,
			  char const *proposed_name,
			  tagi_t const *tags)
{
  if (master_sc == NULL ||
      master_sc->sc_cc == NULL ||
      !su_casematch(compression, tport_sigcomp_name))
    return NULL;

  return tport_sigcomp_name;
}

/** Check if transport can receive compressed messages */
int vsc_can_recv_sigcomp(tport_sigcomp_t const *sc)
{
  return sc && sc->sc_infmt != format_is_noncomp;
}

/** Check if transport can send compressed messages */
int vsc_can_send_sigcomp(tport_sigcomp_t const *sc)
{
  return sc && sc->sc_outfmt != format_is_noncomp;
}

/** Set/reset compression */
int vsc_set_compression(tport_t *self,
			tport_sigcomp_t *sc,
			char const *comp)
{
  if (self == NULL)
    ;
  else if (comp == NULL) {
    if (sc == NULL || sc->sc_outfmt != format_is_sigcomp) {
      self->tp_name->tpn_comp = NULL;
      return 0;
    }
  }
  else {
    comp = tport_canonize_comp(comp);

    if (comp && sc && sc->sc_outfmt != format_is_noncomp) {
      self->tp_name->tpn_comp = comp;
      return 0;
    }
  }

  return -1;
}


/** Assign a SigComp compartment (to a possibly connected tport). */
int tport_sigcomp_assign(tport_t *self, struct sigcomp_compartment *cc)
{
  if (tport_is_connection_oriented(self) &&
      tport_is_secondary(self) &&
      self->tp_socket != INVALID_SOCKET) {

    if (self->tp_sigcomp->sc_cc) {
      if (cc == self->tp_sigcomp->sc_cc)
	return 0;

      /* Remove old assignment */
      sigcomp_compartment_unref(self->tp_sigcomp->sc_cc);
    }

    self->tp_sigcomp->sc_cc = sigcomp_compartment_ref(cc);

    return 0;
  }

  return su_seterrno(EINVAL);
}


void vsc_sigcomp_shutdown(tport_t *self, int how)
{
  if (self->tp_socket != -1)
    shutdown(self->tp_socket, how - 1);

  if (how >= 2 && self->tp_sigcomp->sc_cc) {
    sigcomp_compartment_unref(self->tp_sigcomp->sc_cc);
    self->tp_sigcomp->sc_cc = NULL;
  }
}

static int vsc_recv_sigcomp_r(tport_t*, msg_t**, struct sigcomp_udvm*, int);
static struct sigcomp_compartment *vsc_primary_compartment(tport_master_t *);


static int tport_sigcomp_init_secondary()
{
  if (accept) {
    if (!pri->pri_primary->tp_name->tpn_comp)
      self->tp_sigcomp->sc_infmt = format_is_noncomp;
  }
  else {
    /* XXX - no tpn here */
    if (tpn->tpn_comp == pri->pri_primary->tp_name->tpn_comp)
      self->tp_name->tpn_comp = pri->pri_primary->tp_name->tpn_comp;
    if (!pri->pri_primary->tp_name->tpn_comp)
      self->tp_sigcomp->sc_infmt = format_is_noncomp;
  }
}

static int tport_sigcomp_deinit_secondary(tport_t *self)
{
  if (self->tp_sigcomp) {
    tport_sigcomp_t *sc = self->tp_sigcomp;

    if (sc->sc_udvm)
      sigcomp_udvm_free(sc->sc_udvm), sc->sc_udvm = NULL;
    if (sc->sc_compressor)
      sigcomp_compressor_free(sc->sc_compressor), sc->sc_compressor = NULL;
  }
}

#if 0
#if HAVE_SIGCOMP
  if (tport_can_recv_sigcomp(self))
    return tport_recv_sigcomp_stream(self);
#endif
#endif

/** Try to receive stream data using SigComp. */
static int tport_recv_sigcomp_stream(tport_t *self)
{
  struct sigcomp_udvm *udvm;
  int retval;

  SU_DEBUG_7(("%s(%p)\n", __func__, self));

  /* Peek for first byte in stream,
     determine if this is a compressed stream or not */
  if (self->tp_sigcomp->sc_infmt == format_is_unknown) {
    unsigned char sample;
    int n;

    n = recv(self->tp_socket, &sample, 1, MSG_PEEK);
    if (n < 0)
      return n;

    if (n == 0) {
      recv(self->tp_socket, &sample, 1, 0);
      return 0;			/* E-o-S from first message */
    }

    if ((sample & 0xf8) != 0xf8) {
      /* Not SigComp, receive as usual */
      if (tport_is_primary(self)) {
	SU_DEBUG_1(("%s(%p): receive semantics not implemented\n",
		    __func__, self));
	su_seterrno(EINVAL);		/* Internal error */
	return -1;
      }

      /* Do not try to receive with sigcomp from this socket */
      self->tp_sigcomp->sc_infmt = format_is_noncomp;

      return tport_recv_stream(self);
    }

    /* SigComp, receive using UDVM */
    self->tp_sigcomp->sc_infmt = format_is_sigcomp;

    /* Initialize UDVM */
    self->tp_sigcomp->sc_udvm = tport_init_udvm(self);
    if (!self->tp_sigcomp->sc_udvm) {
      int save = su_errno();
      recv(self->tp_socket, &sample, 1, 0); /* remove msg from socket */
      return su_seterrno(save);
    }
  }

  udvm = self->tp_sigcomp->sc_udvm; assert(udvm);

  retval = tport_recv_sigcomp_r(self, &self->tp_msg, udvm, N);

  if (retval < 0)
    sigcomp_udvm_reject(udvm);

  return retval;
}

/** Receive data available on the socket.
 *
 * @retval -1 error
 * @retval 0  end-of-stream
 * @retval 1  normal receive
 * @retval 2  incomplete recv, recv again
 */
static int tport_recv_sigcomp_r(tport_t *self,
				msg_t **mmsg,
				struct sigcomp_udvm *udvm,
				int N)
{
  msg_t *msg;
  size_t n, m, i;
  int eos, complete;
  ssize_t veclen;
  msg_iovec_t iovec[msg_n_fragments] = {{ 0 }};
  su_sockaddr_t su[1];
  socklen_t su_size = sizeof(su);
  struct sigcomp_buffer *input, *output;
  void *data;
  size_t dlen;

  SU_DEBUG_7(("%s(%p)\n", __func__, self));

  assert(udvm);

  if (sigcomp_udvm_has_input(udvm)) {
    input = sigcomp_udvm_input_buffer(udvm, n = N = 0); assert(input);
  }
  else {
    if (N == 0) {
      assert(self->tp_addrinfo->ai_socktype != SOCK_DGRAM);
      if (self->tp_addrinfo->ai_socktype == SOCK_DGRAM) {
	recv(self->tp_socket, (void *)su, 1, 0);
	return 1;
      }
    }

    input = sigcomp_udvm_input_buffer(udvm, N); assert(input);
    if (input == NULL)
      return su_seterrno(EIO);

    data = input->b_data + input->b_avail;
    dlen = input->b_size - input->b_avail;

    if (tport_is_stream(self)) {
      n = recv(self->tp_socket, data, dlen, 0);
    }
    else if (dlen >= N) {
      n = recvfrom(self->tp_socket, data, dlen, 0, &su->su_sa, &su_size);
      SU_CANONIZE_SOCKADDR(su);
    }
    else {
      recvfrom(self->tp_socket, data, dlen, 0, &su->su_sa, &su_size);
      SU_CANONIZE_SOCKADDR(su);
      su_seterrno(EMSGSIZE);		/* Protocol error */
      return -1;
    }

    if (n == (unsigned)-1) {
      char const *pn = self->tp_protoname;
      int err = su_errno();

      if (su_is_blocking(err)) {
	SU_DEBUG_7(("%s(%p): recv from %s: EAGAIN\n", __func__, self, pn));
	return 1;
      }

      SU_DEBUG_1(("%s(%p): recv from %s: %s (%d)\n", __func__, self, pn,
		  su_strerror(err), err));
      return -1;
    }

    /* XXX - in case of stream, use message buffers for output? */

    input->b_avail += n;
    input->b_complete = (n == 0) || !tport_is_stream(self);
  }

  for (complete = eos = 0; !complete;) {
    output = sigcomp_udvm_output_buffer(udvm, 16384);

    if (sigcomp_udvm_decompress(udvm, output, input) < 0) {
      int error = sigcomp_udvm_errno(udvm);

      SU_DEBUG_3(("%s: UDVM error %d: %s\n", __func__,
		  error, sigcomp_udvm_strerror(udvm)));

      su_seterrno(EREMOTEIO);

      return -1;
    }

    data = output->b_data + output->b_used;
    dlen = output->b_avail - output->b_used;
    complete = output->b_complete;
    eos = complete && input->b_complete;

    veclen = tport_recv_iovec(self, mmsg, iovec, dlen, eos);

    if (dlen ? veclen <= 0 : veclen < 0) {
      return -1;
    }

    for (i = 0, n = 0; i < veclen; i++) {
      m = iovec[i].mv_len; assert(dlen >= n + m);
      memcpy(iovec[i].mv_base, data + n, m);
      n += m;
    }
    assert(dlen == n);

    msg = *mmsg;

    if (msg) {
      /* Message address */
      if (self->tp_addrinfo->ai_socktype == SOCK_STREAM)
	msg_set_address(msg, self->tp_addr, self->tp_addrlen);
      else
	msg_set_address(msg, su, su_size);

      SU_DEBUG_5(("%s(%p): sigcomp recv = %u => %u %s\n", __func__, self,
		  N, dlen, eos ? " (complete)" : ""));

      msg_mark_as_compressed(msg);

      /* Write the received data to the message dump file */
      if (self->tp_master->mr_dump_file && !self->tp_pri->pri_threadpool)
	tport_dump_iovec(self, msg, n, iovec, veclen, "recv", "from");

      /* Send the received data to the capture server */
      if (self->tp_master->mr_capt_sock && !self->tp_pri->pri_threadpool)
	tport_dump_iovec(self, msg, 0);

      msg_recv_commit(msg, dlen, eos);    /* Mark buffer as used */
    }
    else {
      SU_DEBUG_5(("%s(%p): sigcomp recv = %u => %u %s\n", __func__, self,
		  N, dlen, eos ? " (complete)" : ""));
      if (complete || !tport_is_stream(self)) {
	sigcomp_udvm_reject(udvm); /* Reject empty message */
      }
    }

    if (self->tp_addrinfo->ai_socktype == SOCK_STREAM) {
      if (eos)
	return 0;

      if (output->b_complete)
	return n < N || sigcomp_udvm_has_pending_data(udvm) ? 2 : 1;

      if (!sigcomp_udvm_has_input(udvm))
	return 1;
    }
  }

  return eos ? 0 : 2;
}

static
int vsc_send_sigcomp(tport_t const *self,
		     msg_t *msg,
		     msg_iovec_t iov[],
		     int iovused,
		     struct sigcomp_compartment *cc,
		     tport_sigcomp_t *sc)
{
  struct sigcomp_compressor *c = sc->sc_compressor;
  struct sigcomp_buffer *input = sc->sc_input;
  struct sigcomp_buffer *output = sc->sc_output;
  msg_iovec_t ciov[1];

  int i, n, m, k, stream = tport_is_stream(self);
  char const *ccname;
  int ccnamelen;

  su_addrinfo_t *ai = msg_addrinfo(msg);

  int compress = (cc || (cc = sc->sc_cc)) && ai->ai_flags & TP_AI_COMPRESSED;

  if (!compress) {
    if (stream)
      sc->sc_outfmt = format_is_noncomp;
    ai->ai_flags &= ~TP_AI_COMPRESSED;
    return self->tp_pri->pri_vtable->vtp_send(self, msg, iov, iovused, NULL);
  }

  if (stream)
    sc->sc_outfmt = format_is_sigcomp;

  assert(cc);

  if (c == NULL) {
    assert(input == NULL);
    if (stream)
      c = sigcomp_compressor_create_for_stream(cc);
    else
      c = sigcomp_compressor_create(cc);
    sc->sc_compressor = c;
  }

  ccname = sigcomp_compartment_name(cc, &ccnamelen);

  if (sc->sc_compressed != 0) {
    input = NONE;
  }
  else if (input == NULL) {
    int input_size = -1;

    if (tport_is_udp(self)) {
      input_size = 0;

      for (i = 0; i < iovused; i++)
	input_size += iov[i].siv_len;
    }

    sc->sc_input = input = sigcomp_compressor_input_buffer(c, input_size);

    assert(input->b_avail == 0 && input->b_used == 0);
  }
  else if (!input->b_complete) {
    int input_size = 0;

    for (i = 0; i < iovused; i++)
      input_size += iov[i].siv_len;

    if (input_size > input->b_size - input->b_avail)
      sigcomp_buffer_align_available(input, 0);
  }

  if (output == NULL)
    sc->sc_output = output = sigcomp_compressor_output_buffer(c, NULL);

  if (!c || !input || !output) {
    SU_DEBUG_3(("%s(%p): %s (%u)%s%s%s\n",
		__func__, self, strerror(errno), errno,
		c ? "" : " (comp)",
		input ? "" : " (input)",
		output ? "" : " (output)"));
    sigcomp_compressor_free(c);
    sc->sc_compressor = NULL;
    sc->sc_output = NULL; sc->sc_input = NULL;
    sc->sc_compressed = 0; sc->sc_copied = 0;
    return -1;
  }

  if (sc->sc_compressed == 0) {
    k = sc->sc_copied;

    if (!input->b_complete) {
      int m = sc->sc_copied;

      for (i = 0, n = 0; i < iovused; i++) {
	char *b = iov[i].siv_base;
	int l = iov[i].siv_len;

	if (m >= l) {
	  m -= l;
	  continue;
	}

	b += m; l -= m;

	if (input->b_size == input->b_avail)
	  break;

	if (l > input->b_size - input->b_avail)
	  l = input->b_size - input->b_avail;

	memcpy(input->b_data + input->b_avail, b, l);
	input->b_avail += l; n += l; sc->sc_copied += l;

	if (l != iov[i].siv_len)
	  break;
      }
      input->b_complete = i == iovused;
      assert(stream || input->b_complete); (void)stream;
    }

    m = output->b_avail - output->b_used;

    n = sigcomp_compressor_compress(c, output, input);

    if (n < 0) {
      SU_DEBUG_3(("%s(%p): %s (%u)\n", __func__, self,
		  sigcomp_compressor_strerror(c),
		  sigcomp_compressor_errno(c)));
      sigcomp_compressor_free(c);
      sc->sc_compressor = NULL;
      sc->sc_output = NULL; sc->sc_input = NULL;
      sc->sc_compressed = 0;
      return -1;
    }

    assert(input->b_complete || sc->sc_copied - k > 0);

    SU_DEBUG_5(("%s: input %u (%u new) compressed %u to %u with '%.*s'\n",
		__func__, sc->sc_copied, k, n,
		(output->b_avail - output->b_used) - m,
		ccnamelen, ccname));

    sc->sc_compressed = n;

    assert(stream || output->b_complete);
  }
  else {
    assert(tport_is_connection_oriented(self));
    n = sc->sc_compressed;
  }

  assert(input && cc && c && output);

  ciov->siv_base = output->b_data + output->b_used;
  ciov->siv_len = output->b_avail - output->b_used;

  m = self->tp_pri->pri_vtable->vtp_send(self, msg, ciov, 1);

  if (m == -1) {
    int error = su_errno();

    if (su_is_blocking(error)) {
      sigcomp_compressor_free(c);
      sc->sc_compressor = NULL;
      sc->sc_output = NULL; sc->sc_input = NULL;
      sc->sc_compressed = 0; sc->sc_copied = 0;
      su_seterrno(error);
    }

    return -1;
  }

  output->b_used += m;

  if (output->b_used < output->b_avail)
    return 0;

  if (output->b_complete) {
    sigcomp_compressor_accept(c, cc), sc->sc_output = output = NULL;
  }

  if (input != NONE && input->b_avail == input->b_used && input->b_complete)
    sigcomp_buffer_reset(input, -1), sc->sc_input = input = NULL;

  if (!input && !output) {
    sigcomp_compressor_free(c);
    sc->sc_compressor = NULL;
  }

  assert(sc->sc_compressed >= n); assert(sc->sc_copied >= n);

  sc->sc_compressed -= n;
  sc->sc_copied -= n;

  return n;
}

/** Initialize UDVM */
static
struct sigcomp_udvm *tport_init_udvm(tport_t *self)
{
  struct sigcomp_compartment *cc;
  struct sigcomp_udvm *udvm;

  if (self->tp_sigcomp->sc_udvm)
    return self->tp_sigcomp->sc_udvm;

  cc = tport_primary_compartment(self->tp_master);

  if (!cc)
    return NULL;

  if (self->tp_addrinfo->ai_socktype == SOCK_STREAM)
    udvm = sigcomp_udvm_create_for_stream(cc);
  else
    udvm = sigcomp_udvm_create_for_compartment(cc);

  return udvm;
}


/** Get primary compartment */
static
struct sigcomp_compartment *
tport_primary_compartment(tport_master_t *mr)
{
  return mr->mr_compartment;
}

/** Test if tport has a SigComp compartment is assigned to it. */
int vsc_has_sigcomp_assigned(tport_sigcomp_t const *sc)
{
  return sc && sc->sc_udvm != NULL;
}

static
void vsc_try_accept_sigcomp(tport_t *self, msg_t *msg)
{
  struct sigcomp_udvm *udvm;

  udvm = self->tp_sigcomp->sc_udvm;
  if (udvm && sigcomp_udvm_is_complete(udvm)) {
    if (self->tp_master->mr_tpac->tpac_sigcomp_accept &&
	self->tp_sigcomp->sc_cc == NULL) {
      tport_t *ref;
      struct tport_delivery *d;

      d = self->tp_master->mr_delivery;

      d->d_tport = self;
      d->d_msg = msg;
      d->d_udvm = &self->tp_sigcomp->sc_udvm;
      *d->d_from = *self->tp_name;

      ref = tport_incref(self);
      STACK_SIGCOMP_ACCEPT(self, msg);
      /* Reject by default */
      if (self->tp_sigcomp->sc_udvm)
	sigcomp_udvm_accept(self->tp_sigcomp->sc_udvm, NULL);
      tport_decref(&ref);

      d->d_msg = NULL;
    }
    else {
      if (tport_log->log_level >= 5) {
	char const *name;
	int namelen;

	name = sigcomp_compartment_name(self->tp_sigcomp->sc_cc, &namelen);
	SU_DEBUG_5(("tport(%p): msg %p SigComp implicit accept '%.*s'\n",
		    self, msg, namelen, name));
      }
      sigcomp_udvm_accept(udvm, self->tp_sigcomp->sc_cc);
    }
  }
}


/** Accept a SigComp message */
int
tport_sigcomp_accept(tport_t *self,
		     struct sigcomp_compartment *cc,
		     msg_t *msg)
{
  struct sigcomp_udvm *udvm;

  if (self == NULL || msg != self->tp_master->mr_delivery->d_msg)
    return su_seterrno(EINVAL);

  if (!self->tp_master->mr_delivery->d_udvm || cc == NONE)
    return 0;

  udvm = *self->tp_master->mr_delivery->d_udvm;

  if (udvm) {
    if (tport_log->log_level >= 5) {
      char const *name;
      int namelen;

      if (cc) {
	name = sigcomp_compartment_name(cc, &namelen);
	SU_DEBUG_5(("tport(%p): msg %p SigComp accept '%.*s'\n",
		    self, msg, namelen, name));
      }
      else {
	SU_DEBUG_5(("tport(%p): msg %p SigComp reject\n", self, msg));
      }
    }
    sigcomp_udvm_accept(udvm, cc);
  }

  self->tp_master->mr_delivery->d_udvm = NULL;

  return 0;
}


/** Pass message to the protocol stack */
void
tport_sigcomp_deliver(tport_t *self, msg_t *msg, su_time_t now)
{
  /* XXX - no d */

  STACK_RECV(self, msg, now);

  if (d->d_udvm && *d->d_udvm)
    sigcomp_udvm_accept(*d->d_udvm, NULL); /* reject */
}


#if HAVE_SIGCOMP && 0

su_inline
int msg_is_compressed(msg_t *msg)
{
  return msg &&
    (msg_addrinfo(msg)->ai_flags & TP_AI_COMPRESSED) == TP_AI_COMPRESSED;
}

su_inline
void msg_mark_as_compressed(msg_t *msg)
{
  if (msg)
    msg_addrinfo(msg)->ai_flags |= TP_AI_COMPRESSED;
}


struct sigcomp_udvm **tport_get_udvm_slot(tport_t *self)
{
  tport_sigcomp_vtable_t const *vsc = tport_sigcomp_vtable;

  if (vsc)

#if HAVE_SIGCOMP
  return &self->tp_sigcomp->sc_udvm;
#else
  return NULL;
#endif
}

struct sigcomp_compartment *
tport_sigcomp_assign_if_needed(tport_t *self,
			       struct sigcomp_compartment *cc)
{
  if (self->tp_name->tpn_comp) {
    if (cc)
      tport_sigcomp_assign(self, cc);
    else if (self->tp_sigcomp->sc_cc)
      cc = self->tp_sigcomp->sc_cc;
    else
      /* Use default compartment */
      cc = self->tp_master->mr_compartment;
  }
  else
    cc = NULL;

  return cc;
}

/** Receive data from datagram using SigComp. */
int tport_recv_sigcomp_dgram(tport_t *self, int N)
{
  char dummy[1];
  int error = EBADMSG;
#if HAVE_SIGCOMP
  struct sigcomp_udvm *udvm;

  if (self->tp_sigcomp->sc_udvm == 0)
    self->tp_sigcomp->sc_udvm = tport_init_udvm(self);

  udvm = self->tp_sigcomp->sc_udvm;

  if (udvm) {
    retval = tport_recv_sigcomp_r(self, &self->tp_msg, udvm, N);
    if (retval < 0)
      sigcomp_udvm_reject(udvm);
    return retval;
  }
  error = su_errno();
#endif
  recv(self->tp_socket, dummy, 1, 0); /* remove msg from socket */
  /* XXX - send NACK ? */
  return su_seterrno(error);
}
