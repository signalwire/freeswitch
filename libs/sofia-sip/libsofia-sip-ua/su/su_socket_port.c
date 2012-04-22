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

/**@ingroup su_wait
 * @CFILE su_socket_port.c
 *
 * OS-Independent Syncronization Interface with socket mailbox
 *
 * This implements wakeup using sockets by su_port_send().
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define su_socket_port_s su_port_s
#define SU_CLONE_T su_msg_t

#include "sofia-sip/su.h"
#include "su_port.h"
#include "sofia-sip/su_alloc.h"

#if HAVE_SOCKETPAIR
#define SU_MBOX_SEND 1
#else
#define SU_MBOX_SEND 0
#endif

/** @internal Message box wakeup function. */
static int su_mbox_port_wakeup(su_root_magic_t *magic, /* NULL */
			       su_wait_t *w,
			       su_wakeup_arg_t *arg)
{
  char buf[32];
  su_socket_t socket = *(su_socket_t*)arg;
  su_wait_events(w, socket);
  recv(socket, buf, sizeof(buf), 0);
  return 0;
}

/**@internal
 *
 * Initializes a message port. It creates a mailbox used to wake up the
 * thread waiting on the port if needed. Currently, the mailbox is a
 * socketpair or an UDP socket connected to itself.
 */
int su_socket_port_init(su_port_t *self, su_port_vtable_t const *vtable)
{
  int retval = -1;
  int af;
  su_socket_t mb = INVALID_SOCKET;
  su_wait_t wait[1] = { SU_WAIT_INIT };
  char const *why;

  SU_DEBUG_9(("su_socket_port_init(%p, %p) called\n",
	      (void *)self, (void *)vtable));

  if (su_pthread_port_init(self, vtable) != 0)
    return -1;

#if HAVE_SOCKETPAIR
#if defined(AF_LOCAL)
  af = AF_LOCAL;
#else
  af = AF_UNIX;
#endif

  if (socketpair(af, SOCK_STREAM, 0, self->sup_mbox) == -1) {
    why = "socketpair"; goto error;
  }

  mb = self->sup_mbox[0];
  su_setblocking(self->sup_mbox[1], 0);

#else
  {
    struct sockaddr_in sin = { sizeof(struct sockaddr_in), 0 };
    socklen_t sinsize = sizeof sin;
    struct sockaddr *sa = (struct sockaddr *)&sin;

    af = PF_INET;

    self->sup_mbox[0] = mb = su_socket(af, SOCK_DGRAM, IPPROTO_UDP);
    if (mb == INVALID_SOCKET) {
      why = "socket"; goto error;
    }

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.1 */

    /* Get a port for us */
    if (bind(mb, sa, sizeof sin) == -1) {
      why = "bind"; goto error;
    }

    if (getsockname(mb, sa, &sinsize) == -1) {
      why = "getsockname"; goto error;
    }

    if (connect(mb, sa, sinsize) == -1) {
      why = "connect"; goto error;
    }
  }
#endif

  if (su_wait_create(wait, mb, SU_WAIT_IN) == -1) {
    why = "su_wait_create";
    goto error;
  }

  self->sup_mbox_index = su_port_register(self, NULL, wait,
					  su_mbox_port_wakeup,
					  (void *)self->sup_mbox, 0);

  if (self->sup_mbox_index <= 0) {
    why = "su_port_register";
    su_wait_destroy(wait);
    goto error;
  }

  return 0;

  error:
    su_log("%s: %s: %s\n", "su_socket_port_init",
	   why, su_strerror(su_errno()));

  return retval;
}


/** @internal Deinit a base implementation of port. */
void su_socket_port_deinit(su_port_t *self)
{
  assert(self);

  if (self->sup_mbox_index > 0)
    su_port_deregister(self, self->sup_mbox_index);
  self->sup_mbox_index = 0;

  if (self->sup_mbox[0] && self->sup_mbox[0] != INVALID_SOCKET)
    su_close(self->sup_mbox[0]); self->sup_mbox[0] = INVALID_SOCKET;
#if HAVE_SOCKETPAIR
  if (self->sup_mbox[1] && self->sup_mbox[1] != INVALID_SOCKET)
    su_close(self->sup_mbox[1]); self->sup_mbox[1] = INVALID_SOCKET;
#endif

  su_pthread_port_deinit(self);
}

/** @internal Wake up the port. */
int su_socket_port_wakeup(su_port_t *self)
{
  assert(self->sup_mbox[SU_MBOX_SEND] != INVALID_SOCKET);

  if (!su_port_own_thread(self) &&
      send(self->sup_mbox[SU_MBOX_SEND], "X", 1, 0) == -1) {
#if HAVE_SOCKETPAIR
    if (su_errno() != EWOULDBLOCK)
#endif
      su_perror("su_msg_send: send()");
  }

  return 0;
}
