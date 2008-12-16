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
 *
 * @date Split here: Fri Mar 24 08:45:49 EET 2006 ppessi
 * @date Originally Created: Thu Jul 20 12:54:32 2000 ppessi
 */

#include "config.h"

#include "tport_internal.h"

#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <sofia-sip/string0.h>

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

typedef struct
{
  tport_primary_t tlspri_pri[1];
  tls_t *tlspri_master;
} tport_tls_primary_t;

typedef struct
{
  tport_t tlstp_tp[1];
  tls_t  *tlstp_context;
  char   *tlstp_buffer;    /**< 2k Buffer  */
} tport_tls_t;

tport_vtable_t const tport_tls_vtable =
{
  "tls", tport_type_local,
  sizeof (tport_tls_primary_t),
  tport_tls_init_primary,
  tport_tls_deinit_primary,
  tport_accept,
  NULL,
  sizeof (tport_tls_t),
  tport_tls_init_secondary,
  tport_tls_deinit_secondary,
  tport_tls_shutdown,
  tport_tls_set_events,
  tport_tls_events,
  tport_tls_recv,
  tport_tls_send,
};

tport_vtable_t const tport_tls_client_vtable =
{
  "tls", tport_type_client,
  sizeof (tport_tls_primary_t),
  tport_tls_init_client,
  tport_tls_deinit_primary,
  tport_accept,
  NULL,
  sizeof (tport_tls_t),
  tport_tls_init_secondary,
  tport_tls_deinit_secondary,
  tport_tls_shutdown,
  tport_tls_set_events,
  tport_tls_events,
  tport_tls_recv,
  tport_tls_send,
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
  unsigned tls_version = 1;
  unsigned tls_verify = 0;
  su_home_t autohome[SU_HOME_AUTO_SIZE(1024)];
  tls_issues_t ti = {0};

  su_home_auto(autohome, sizeof autohome);

  if (getenv("TPORT_SSL"))
    tls_version = 0;

  tl_gets(tags,
	  TPTAG_CERTIFICATE_REF(path),
	  TPTAG_TLS_VERSION_REF(tls_version),
	  TPTAG_TLS_VERIFY_PEER_REF(tls_verify),
	  TAG_END());

  if (!path) {
    homedir = getenv("HOME");
    if (!homedir)
      homedir = "";
    path = tbf = su_sprintf(autohome, "%s/.sip/auth", homedir);
  }

  if (path) {
    ti.verify_peer = tls_verify;
    ti.verify_depth = 2;
    ti.configured = path != tbf;
    ti.randFile = su_sprintf(autohome, "%s/%s", path, "tls_seed.dat");
    ti.key = su_sprintf(autohome, "%s/%s", path, "agent.pem");
    ti.cert = ti.key;
    ti.CAfile = su_sprintf(autohome, "%s/%s", path, "cafile.pem");
    ti.version = tls_version;
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
    if (!path || ti.configured) {
      SU_DEBUG_1(("tls_init_master: %s\n", strerror(errno)));
    }
    else {
      SU_DEBUG_5(("tls_init_master: %s\n", strerror(errno)));
    }
    return *return_culprit = "tls_init_master", -1;
  }

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

  if (accepted) {
    tlstp->tlstp_context = tls_init_slave(master, socket);
    if (!tlstp->tlstp_context)
      return *return_reason = "tls_init_slave", -1;
  }

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
  tport_tls_primary_t *tlspri = (tport_tls_primary_t *)self->tp_pri;
  tport_tls_t *tlstp = (tport_tls_t *)self;
  enum { TLSBUFSIZE = 2048 };
  size_t i, j, n, m, size = 0;
  ssize_t nerror;
  int oldmask, mask;

  if (tlstp->tlstp_context == NULL) {
    tls_t *master = tlspri->tlspri_master;
    tlstp->tlstp_context = tls_init_client(master, self->tp_socket);
    if (!tlstp->tlstp_context)
      return -1;
  }

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
