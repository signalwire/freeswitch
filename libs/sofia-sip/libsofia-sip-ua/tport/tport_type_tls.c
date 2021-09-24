/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005 Nokia Corporation.
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

/**@CFILE tport_type_tls.c TLS over TCP Transport
 *
 * See tport.docs for more detailed description of tport interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Ismo Puustinen <Ismo.H.Puustinen@nokia.com>
 * @author Tat Chan <Tat.Chan@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Jarod Neuner <janeuner@networkharbor.com>
 *
 * @date Split here: Fri Mar 24 08:45:49 EET 2006 ppessi
 * @date Originally Created: Thu Jul 20 12:54:32 2000 ppessi
 */

#include "config.h"

#define SU_WAKEUP_ARG_T  struct tport_s

#include "tport_internal.h"

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sofia-sip/su_string.h>

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "tport_type_tls";
#endif

#if HAVE_WIN32
#include <io.h>
#define access(_filename, _mode) _access(_filename, _mode)
#define R_OK (04)
#endif

/* ---------------------------------------------------------------------- */
/* TLS */

#include "tport_tls.h"

static int tport_tls_init_primary(tport_primary_t *,
				  tp_name_t tpn[1],
				  su_addrinfo_t *, tagi_t const *,
				  char const **return_culprit);
static int tport_tls_init_client(tport_primary_t *,
				 tp_name_t tpn[1],
				 su_addrinfo_t *, tagi_t const *,
				 char const **return_culprit);
static int tport_tls_init_master(tport_primary_t *pri,
				 tp_name_t tpn[1],
				 su_addrinfo_t *ai,
				 tagi_t const *tags,
				 char const **return_culprit);
static void tport_tls_deinit_primary(tport_primary_t *pri);
static int tport_tls_init_secondary(tport_t *self, int socket, int accepted,
				    char const **return_reason);
static void tport_tls_deinit_secondary(tport_t *self);
static void tport_tls_shutdown(tport_t *self, int how);
static int tport_tls_set_events(tport_t const *self);
static int tport_tls_events(tport_t *self, int events);
static int tport_tls_recv(tport_t *self);
static ssize_t tport_tls_send(tport_t const *self, msg_t *msg,
			      msg_iovec_t iov[], size_t iovused);
static int tport_tls_accept(tport_primary_t *pri, int events);
static tport_t *tport_tls_connect(tport_primary_t *pri, su_addrinfo_t *ai,
                                  tp_name_t const *tpn);

tport_vtable_t const tport_tls_vtable =
{
  /* vtp_name 		     */ "tls",
  /* vtp_public              */ tport_type_local,
  /* vtp_pri_size            */ sizeof (tport_tls_primary_t),
  /* vtp_init_primary        */ tport_tls_init_primary,
  /* vtp_deinit_primary      */ tport_tls_deinit_primary,
  /* vtp_wakeup_pri          */ tport_tls_accept,
  /* vtp_connect             */ tport_tls_connect,
  /* vtp_secondary_size      */ sizeof (tport_tls_t),
  /* vtp_init_secondary      */ tport_tls_init_secondary,
  /* vtp_deinit_secondary    */ tport_tls_deinit_secondary,
  /* vtp_shutdown            */ tport_tls_shutdown,
  /* vtp_set_events          */ tport_tls_set_events,
  /* vtp_wakeup              */ tport_tls_events,
  /* vtp_recv                */ tport_tls_recv,
  /* vtp_send                */ tport_tls_send,
  /* vtp_deliver             */ NULL,
  /* vtp_prepare             */ NULL,
  /* vtp_keepalive           */ NULL,
  /* vtp_stun_response       */ NULL,
  /* vtp_next_secondary_timer*/ NULL,
  /* vtp_secondary_timer     */ NULL,
};

tport_vtable_t const tport_tls_client_vtable =
{
  /* vtp_name 		     */ "tls",
  /* vtp_public              */ tport_type_client,
  /* vtp_pri_size            */ sizeof (tport_tls_primary_t),
  /* vtp_init_primary        */ tport_tls_init_client,
  /* vtp_deinit_primary      */ tport_tls_deinit_primary,
  /* vtp_wakeup_pri          */ tport_tls_accept,
  /* vtp_connect             */ tport_tls_connect,
  /* vtp_secondary_size      */ sizeof (tport_tls_t),
  /* vtp_init_secondary      */ tport_tls_init_secondary,
  /* vtp_deinit_secondary    */ tport_tls_deinit_secondary,
  /* vtp_shutdown            */ tport_tls_shutdown,
  /* vtp_set_events          */ tport_tls_set_events,
  /* vtp_wakeup              */ tport_tls_events,
  /* vtp_recv                */ tport_tls_recv,
  /* vtp_send                */ tport_tls_send,
  /* vtp_deliver             */ NULL,
  /* vtp_prepare             */ NULL,
  /* vtp_keepalive           */ NULL,
  /* vtp_stun_response       */ NULL,
  /* vtp_next_secondary_timer*/ NULL,
  /* vtp_secondary_timer     */ NULL,
};

static int tport_tls_init_primary(tport_primary_t *pri,
				  tp_name_t tpn[1],
				  su_addrinfo_t *ai,
				  tagi_t const *tags,
				  char const **return_culprit)
{
  if (tport_tls_init_master(pri, tpn, ai, tags, return_culprit) < 0)
    return -1;

  return tport_tcp_init_primary(pri, tpn, ai, tags, return_culprit);
}

static int tport_tls_init_client(tport_primary_t *pri,
				 tp_name_t tpn[1],
				 su_addrinfo_t *ai,
				 tagi_t const *tags,
				 char const **return_culprit)
{
  if (tport_tls_init_master(pri, tpn, ai, tags, return_culprit) < 0)
    return -1;

  return tport_tcp_init_client(pri, tpn, ai, tags, return_culprit);
}

static int tport_tls_init_master(tport_primary_t *pri,
				 tp_name_t tpn[1],
				 su_addrinfo_t *ai,
				 tagi_t const *tags,
				 char const **return_culprit)
{
  tport_tls_primary_t *tlspri = (tport_tls_primary_t *)pri;
  char *homedir;
  char *tbf = NULL;
  char const *path = NULL;
  char const *tls_ciphers = NULL;
  unsigned tls_version = 1;
  unsigned tls_timeout = 300;
  unsigned tls_verify = 0;
  char const *passphrase = NULL;
  unsigned tls_policy = TPTLS_VERIFY_NONE;
  unsigned tls_depth = 0;
  unsigned tls_date = 1;
  su_strlst_t const *tls_subjects = NULL;
  su_home_t autohome[SU_HOME_AUTO_SIZE(1024)];
  tls_issues_t ti = {0};

  su_home_auto(autohome, sizeof autohome);

  if (getenv("TPORT_SSL"))
    tls_version = 0;

  tl_gets(tags,
	  TPTAG_CERTIFICATE_REF(path),
	  TPTAG_TLS_CIPHERS_REF(tls_ciphers),
	  TPTAG_TLS_VERSION_REF(tls_version),
	  TPTAG_TLS_TIMEOUT_REF(tls_timeout),
	  TPTAG_TLS_VERIFY_PEER_REF(tls_verify),
	  TPTAG_TLS_PASSPHRASE_REF(passphrase),
	  TPTAG_TLS_VERIFY_POLICY_REF(tls_policy),
	  TPTAG_TLS_VERIFY_DEPTH_REF(tls_depth),
	  TPTAG_TLS_VERIFY_DATE_REF(tls_date),
	  TPTAG_TLS_VERIFY_SUBJECTS_REF(tls_subjects),
	  TAG_END());

  if (!path) {
    homedir = getenv("HOME");
    if (!homedir)
      homedir = "";
    path = tbf = su_sprintf(autohome, "%s/.sip/auth", homedir);
  }

  if (path) {
    ti.policy = tls_policy | (tls_verify ? TPTLS_VERIFY_ALL : 0);
    ti.verify_depth = tls_depth;
    ti.verify_date = tls_date;
    ti.configured = path != tbf;
    ti.randFile = su_sprintf(autohome, "%s/%s", path, "tls_seed.dat");
    ti.key = su_sprintf(autohome, "%s/%s", path, "agent.pem");
	if (access(ti.key, R_OK) != 0) ti.key = NULL;
    if (!ti.key) ti.key = su_sprintf(autohome, "%s/%s", path, "tls.pem");
    ti.passphrase = su_strdup(autohome, passphrase);
    ti.cert = ti.key;
    ti.CAfile = su_sprintf(autohome, "%s/%s", path, "cafile.pem");
	if (access(ti.CAfile, R_OK) != 0) ti.CAfile = NULL;
    if (!ti.CAfile) ti.CAfile = su_sprintf(autohome, "%s/%s", path, "tls.pem");
    if (tls_ciphers) ti.ciphers = su_strdup(autohome, tls_ciphers);
    ti.version = tls_version;
    ti.timeout = tls_timeout;
    ti.CApath = su_strdup(autohome, path);

    SU_DEBUG_9(("%s(%p): tls key = %s\n", __func__, (void *)pri, ti.key));

    if (ti.key && ti.CAfile && ti.randFile) {
      if (access(ti.key, R_OK) != 0) ti.key = NULL;
      if (access(ti.randFile, R_OK) != 0) ti.randFile = NULL;
      if (access(ti.CAfile, R_OK) != 0) ti.CAfile = NULL;
      tlspri->tlspri_master = tls_init_master(&ti);
    }
  }

  su_home_zap(autohome);

  if (!tlspri->tlspri_master) {
    /*
    if (!path || ti.configured) {
      SU_DEBUG_1(("tls_init_master: %s\n", strerror(errno)));
    }
    else {
      SU_DEBUG_5(("tls_init_master: %s\n", strerror(errno)));
    }
    */
    return *return_culprit = "tls_init_master", -1;
  } else {
    char buf[TPORT_HOSTPORTSIZE];
    su_sockaddr_t *sa = ai ? (void *)(ai->ai_addr) : NULL;
    if (sa && tport_hostport(buf, sizeof(buf), sa, 2))
      SU_DEBUG_5(("%s(%p): tls context initialized for %s\n", \
                  __func__, (void *)pri, buf));
  }

  if (tls_subjects)
    pri->pri_primary->tp_subjects = su_strlst_dup(pri->pri_home, tls_subjects);
  pri->pri_has_tls = 1;

  return 0;
}

static void tport_tls_deinit_primary(tport_primary_t *pri)
{
  tport_tls_primary_t *tlspri = (tport_tls_primary_t *)pri;
  tls_free(tlspri->tlspri_master), tlspri->tlspri_master = NULL;
}

static int tport_tls_init_secondary(tport_t *self, int socket, int accepted,
				    char const **return_reason)
{
  tport_tls_primary_t *tlspri = (tport_tls_primary_t *)self->tp_pri;
  tport_tls_t *tlstp = (tport_tls_t *)self;

  tls_t *master = tlspri->tlspri_master;

  if (tport_tcp_init_secondary(self, socket, accepted, return_reason) < 0)
    return -1;

  tlstp->tlstp_context = tls_init_secondary(master, socket, accepted);
  if (!tlstp->tlstp_context)
    return *return_reason = "tls_init_slave", -1;

  return 0;
}

static void tport_tls_deinit_secondary(tport_t *self)
{
  tport_tls_t *tlstp = (tport_tls_t *)self;

  /* XXX - PPe: does the tls_shutdown zap everything but socket? */
  if (tlstp->tlstp_context != NULL)
    tls_free(tlstp->tlstp_context);
  tlstp->tlstp_context = NULL;

  su_free(self->tp_home, tlstp->tlstp_buffer);
  tlstp->tlstp_buffer = NULL;
}

static void tport_tls_shutdown(tport_t *self, int how)
{
  tport_tls_t *tlstp = (tport_tls_t *)self;

  /* XXX - send alert */
  (void)tlstp;

  shutdown(self->tp_socket, how);

  if (how >= 2)
    tport_tls_deinit_secondary(self);
}


static
int tport_tls_set_events(tport_t const *self)
{
  tport_tls_t *tlstp = (tport_tls_t *)self;
  int mask = tls_events(tlstp->tlstp_context, self->tp_events);

  SU_DEBUG_7(("%s(%p): logical events%s%s real%s%s\n",
	      "tport_tls_set_events", (void *)self,
	      (self->tp_events & SU_WAIT_IN) ? " IN" : "",
	      (self->tp_events & SU_WAIT_OUT) ? " OUT" : "",
	      (mask & SU_WAIT_IN) ? " IN" : "",
	      (mask & SU_WAIT_OUT) ? " OUT" : ""));

  return
    su_root_eventmask(self->tp_master->mr_root,
		      self->tp_index,
		      self->tp_socket,
		      mask);
}

/** Handle poll events for tls */
int tport_tls_events(tport_t *self, int events)
{
  tport_tls_t *tlstp = (tport_tls_t *)self;
  int old_mask = tls_events(tlstp->tlstp_context, self->tp_events), mask;
  int ret, error = 0;

  if (events & SU_WAIT_ERR)
    error = tport_error_event(self);

  if ((self->tp_events & SU_WAIT_OUT) && !self->tp_closed) {
    ret = tls_want_write(tlstp->tlstp_context, events);
    if (ret > 0)
      tport_send_event(self);
    else if (ret < 0)
      tport_error_report(self, errno, NULL);
  }

  if ((self->tp_events & SU_WAIT_IN) && !self->tp_closed) {
    for (;;) {
      ret = tls_want_read(tlstp->tlstp_context, events);
      if (ret > 1) {
	tport_recv_event(self);
	if ((events & SU_WAIT_HUP) && !self->tp_closed)
	  continue;
      }
      break;
    }

    if (ret == 0) { 		/* End-of-stream */
      if (self->tp_msg)
	tport_recv_event(self);
      tport_shutdown0(self, 2);
    }

    if (ret < 0)
      tport_error_report(self, errno, NULL);
  }

  if ((events & SU_WAIT_HUP) && !self->tp_closed)
    tport_hup_event(self);

  if (error && !self->tp_closed)
    tport_error_report(self, error, NULL);

  if (self->tp_closed)
    return 0;

  events = self->tp_events;
  mask = tls_events(tlstp->tlstp_context, events);
  if ((old_mask ^ mask) == 0)
    return 0;

  SU_DEBUG_7(("%s(%p): logical events%s%s real%s%s\n",
	      "tport_tls_events", (void *)self,
	      (events & SU_WAIT_IN) ? " IN" : "",
	      (events & SU_WAIT_OUT) ? " OUT" : "",
	      (mask & SU_WAIT_IN) ? " IN" : "",
	      (mask & SU_WAIT_OUT) ? " OUT" : ""));

  su_root_eventmask(self->tp_master->mr_root,
		    self->tp_index,
		    self->tp_socket,
		    mask);

  return 0;
}

/** Receive data from TLS.
 *
 * @retval -1 error
 * @retval 0  end-of-stream
 * @retval 1  normal receive
 * @retval 2  incomplete recv, recv again
 *
 */
static
int tport_tls_recv(tport_t *self)
{
  tport_tls_t *tlstp = (tport_tls_t *)self;
  msg_t *msg;
  ssize_t n, N, veclen, i, m;
  msg_iovec_t iovec[msg_n_fragments] = {{ 0 }};
  char *tls_buf;

  N = tls_read(tlstp->tlstp_context);

  SU_DEBUG_7(("%s(%p): tls_read() returned "MOD_ZD"\n", __func__, (void *)self, N));

  if (N == 0) {
    if (self->tp_msg)
      msg_recv_commit(self->tp_msg, 0, 1); /* End-of-stream */
    return 0;
  }
  else if (N == -1) {
    if (su_is_blocking(su_errno())) {
      tport_tls_set_events(self);
      return 1;
    }
    return -1;
  }

  veclen = tport_recv_iovec(self, &self->tp_msg, iovec, N, 0);
  if (veclen < 0)
    return -1;

  msg = self->tp_msg;

  tls_buf = tls_read_buffer(tlstp->tlstp_context, N);

  msg_set_address(msg, self->tp_addr, self->tp_addrlen);

  for (i = 0, n = 0; i < veclen; i++) {
    m = iovec[i].mv_len; assert(N >= n + m);
    memcpy(iovec[i].mv_base, tls_buf + n, m);
    n += m;
  }

  assert(N == n);

  /* Write the received data to the message dump file */
  if (self->tp_master->mr_dump_file)
    tport_dump_iovec(self, msg, n, iovec, veclen, "recv", "from");

  if (self->tp_master->mr_capt_sock)
    tport_capt_msg(self, msg, n, iovec, veclen, "recv");

  /* Mark buffer as used */
  msg_recv_commit(msg, N, 0);

  return tls_pending(tlstp->tlstp_context) ? 2 : 1;
}

static
ssize_t tport_tls_send(tport_t const *self,
		       msg_t *msg,
		       msg_iovec_t iov[],
		       size_t iovlen)
{
  tport_tls_t *tlstp = (tport_tls_t *)self;
  enum { TLSBUFSIZE = 2048 };
  size_t i, j, n, m, size = 0;
  ssize_t nerror;
  int oldmask, mask;

  oldmask = tls_events(tlstp->tlstp_context, self->tp_events);

#if 0
  if (!tlstp->tlstp_buffer)
    tlstp->tlstp_buffer = su_alloc(self->tp_home, TLSBUFSIZE);
#endif

  for (i = 0; i < iovlen; i = j) {
#if 0
    nerror = tls_write(tlstp->tlstp_context,
		  iov[i].siv_base,
		  m = iov[i].siv_len);
    j = i + 1;
#else
    char *buf = tlstp->tlstp_buffer;
    unsigned tlsbufsize = TLSBUFSIZE;

    if (i + 1 == iovlen)
      buf = NULL;		/* Don't bother copying single chunk */

    if (buf &&
	(char *)iov[i].siv_base - buf < TLSBUFSIZE &&
	(char *)iov[i].siv_base - buf >= 0) {
      tlsbufsize = buf + TLSBUFSIZE - (char *)iov[i].siv_base;
      assert(tlsbufsize <= TLSBUFSIZE);
    }

    for (j = i, m = 0; buf && j < iovlen; j++) {
      if (m + iov[j].siv_len > tlsbufsize)
	break;
      if (buf + m != iov[j].siv_base)
	memcpy(buf + m, iov[j].siv_base, iov[j].siv_len);
      m += iov[j].siv_len; iov[j].siv_len = 0;
    }

    if (j == i)
      buf = iov[i].siv_base, m = iov[i].siv_len, j++;
    else
      iov[j].siv_base = buf, iov[j].siv_len = m;

    nerror = tls_write(tlstp->tlstp_context, buf, m);
#endif

    SU_DEBUG_9(("tport_tls_writevec: vec %p %p %lu ("MOD_ZD")\n",
		(void *)tlstp->tlstp_context, (void *)iov[i].siv_base, (LU)iov[i].siv_len,
		nerror));

    if (nerror == -1) {
      int err = su_errno();
      if (su_is_blocking(err))
	break;
      SU_DEBUG_3(("tls_write: %s\n", strerror(err)));
      return -1;
    }

    n = (size_t)nerror;
    size += n;

    /* Return if the write buffer is full for now */
    if (n != m)
      break;
  }

  mask = tls_events(tlstp->tlstp_context, self->tp_events);

  if (oldmask != mask)
    tport_tls_set_events(self);

  return size;
}

static
int tport_tls_accept(tport_primary_t *pri, int events)
{
  tport_t *self;
  su_addrinfo_t ai[1];
  su_sockaddr_t su[1];
  socklen_t sulen = sizeof su;
  su_socket_t s = INVALID_SOCKET, l = pri->pri_primary->tp_socket;
  char const *reason = "accept";

  if (events & SU_WAIT_ERR)
    tport_error_event(pri->pri_primary);

  if (!(events & SU_WAIT_ACCEPT))
    return 0;

  memcpy(ai, pri->pri_primary->tp_addrinfo, sizeof ai);
  ai->ai_canonname = NULL;

  s = accept(l, &su->su_sa, &sulen);

  if (s < 0) {
    tport_error_report(pri->pri_primary, su_errno(), NULL);
    return 0;
  }

  ai->ai_addr = &su->su_sa, ai->ai_addrlen = sulen;

  /* Alloc a new transport object, then register socket events with it */
  if ((self = tport_alloc_secondary(pri, s, 1, &reason)) == NULL) {
    SU_DEBUG_3(("%s(%p): incoming secondary on "TPN_FORMAT
                " failed. reason = %s\n", __func__, (void *)pri,
                TPN_ARGS(pri->pri_primary->tp_name), reason));
    return 0;
  }
  else {
    int events = SU_WAIT_IN|SU_WAIT_ERR|SU_WAIT_HUP;

    SU_CANONIZE_SOCKADDR(su);

    if (/* Name this transport */
        tport_setname(self, pri->pri_protoname, ai, NULL) != -1
	/* Register this secondary */
	&&
	tport_register_secondary(self, tls_connect, events) != -1) {

      self->tp_conn_orient = 1;
      self->tp_is_connected = 0;

      SU_DEBUG_5(("%s(%p): new connection from " TPN_FORMAT "\n",
		  __func__, (void *)self, TPN_ARGS(self->tp_name)));

      /* Return succesfully */
      return 0;
    }

    /* Failure: shutdown socket,  */
    tport_close(self);
    tport_zap_secondary(self);
    self = NULL;
  }

  return 0;
}

static
tport_t *tport_tls_connect(tport_primary_t *pri,
                           su_addrinfo_t *ai,
			   tp_name_t const *tpn)
{
  tport_t *self = NULL;

  su_socket_t s, server_socket;
  int events = SU_WAIT_CONNECT | SU_WAIT_ERR;

  int err;
  unsigned errlevel = 3;
  char buf[TPORT_HOSTPORTSIZE];
  char const *what;

  what = "su_socket";
  s = su_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (s == INVALID_SOCKET)
    goto sys_error;

  what = "tport_alloc_secondary";
  if ((self = tport_alloc_secondary(pri, s, 0, &what)) == NULL)
    goto sys_error;

  self->tp_conn_orient = 1;

  if ((server_socket = pri->pri_primary->tp_socket) != INVALID_SOCKET) {
    su_sockaddr_t susa;
    socklen_t susalen = sizeof(susa);

    if (getsockname(server_socket, &susa.su_sa, &susalen) < 0) {
      SU_DEBUG_3(("%s(%p): getsockname(): %s\n",
                  __func__, (void *)self, su_strerror(su_errno())));
    } else {
      susa.su_port = 0;
      if (bind(s, &susa.su_sa, susalen) < 0) {
        SU_DEBUG_3(("%s(%p): bind(local-ip): %s\n",
                    __func__, (void *)self, su_strerror(su_errno())));
      }
    }
  }

  what = "connect";
  if (connect(s, ai->ai_addr, (socklen_t)(ai->ai_addrlen)) == SOCKET_ERROR) {
    err = su_errno();
    if (!su_is_blocking(err))
      goto sys_error;
  }

  what = "tport_setname";
  if (tport_setname(self, tpn->tpn_proto, ai, tpn->tpn_canon) == -1)
    goto sys_error;

  what = "tport_register_secondary";
  if (tport_register_secondary(self, tls_connect, events) == -1)
    goto sys_error;

  SU_DEBUG_5(("%s(%p): connecting to " TPN_FORMAT "\n",
              __func__, (void *)self, TPN_ARGS(self->tp_name)));

  tport_set_secondary_timer(self);

  return self;

sys_error:
  err = errno;
  if (SU_LOG_LEVEL >= errlevel)
    su_llog(tport_log, errlevel, "%s(%p): %s (pf=%d %s/%s): %s\n",
            __func__, (void *)pri, what, ai->ai_family, tpn->tpn_proto,
	    tport_hostport(buf, sizeof(buf), (void *)ai->ai_addr, 2),
	    su_strerror(err));
  tport_zap_secondary(self);
  su_seterrno(err);
  return NULL;
}
