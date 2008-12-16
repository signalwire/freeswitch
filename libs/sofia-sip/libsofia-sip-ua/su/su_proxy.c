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

/**@internal @ingroup su_root_ex
 *
 * @file su_proxy.c
 *
 * @brief Transport level proxy demonstrating various @b su features.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Wed May 23 17:42:40 2001 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>

#include <assert.h>

typedef struct proxy_s proxy_t;
typedef struct forwarder_s forwarder_t;
typedef struct buffer_s buffer_t;

#define SU_ROOT_MAGIC_T proxy_t
#define SU_MSG_ARG_T    su_socket_t
#define SU_WAKEUP_ARG_T forwarder_t

#include "sofia-sip/su.h"
#include "sofia-sip/su_wait.h"
#include "sofia-sip/su_alloc.h"
#include "su_module_debug.h"

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
#define __func__ "su_proxy"
#endif

struct proxy_s
{
  su_home_t      pr_home[1];
  su_root_t     *pr_root;
  su_addrinfo_t *pr_addrinfo;
  forwarder_t   *pr_forwarders;
};

struct forwarder_s
{
  forwarder_t  *f_next;
  forwarder_t **f_prev;
  proxy_t      *f_pr;
  su_socket_t   f_socket;
  su_wait_t     f_wait[2];
  forwarder_t  *f_peer;
  su_sockaddr_t f_dest[1];
  buffer_t     *f_buf;
  unsigned long f_sent;		/* bytes sent */
  unsigned      f_shutdown : 1;
  unsigned      f_upstream : 1;
};

struct buffer_s
{
  buffer_t  *b_next;
  buffer_t **b_prev;
  int        b_sent;
  int        b_used;
  char       b_data[8192];
};

char const help[] =
"usage: su_proxy [-ntu] remotehost remoteport [localport]\n";

void usage(void)
{
  fputs(help, stderr);
}

static int pr_init(proxy_t *pr);
static int pr_config(proxy_t *pr, int argc, char *argv[]);
static int pr_run(proxy_t *pr);
static void pr_deinit(proxy_t *pr);

static forwarder_t *forwarder_create(proxy_t *pr);
static forwarder_t *forwarder_create_listener(proxy_t *pr, su_addrinfo_t *ai);
static void forwarder_deinit(forwarder_t *f);
static int forwarder_init_stream(forwarder_t *f);
static int forwarder_init_dgram(forwarder_t *f);
static int forwarder_accept(proxy_t *pr, su_wait_t *w, forwarder_t *f);
static int forwarder_stream_peer(proxy_t *pr, forwarder_t *f);
static int forwarder_connected(proxy_t *pr, su_wait_t *w, forwarder_t *f);
static int forwarder_recv(proxy_t *pr, su_wait_t *w, forwarder_t *f);
static int forwarder_send(proxy_t *pr, forwarder_t *f, buffer_t *b);
static int forwarder_append(forwarder_t *f, buffer_t *b0);
static int forwarder_empty(proxy_t *pr, su_wait_t *w, forwarder_t *f);
static int forwarder_shutdown(forwarder_t *f);
static void forwarder_close(forwarder_t *f1);

int main(int argc, char *argv[])
{
  proxy_t pr[1] = {{{ SU_HOME_INIT(pr) }}};
  int error;

  error = pr_init(pr);
  if (error == 0) {
    if ((error = pr_config(pr, argc, argv)) > 1)
      usage();
  }

  if (error == 0)
    error = pr_run(pr);

  pr_deinit(pr);

  su_deinit();

  exit(error);
}

int pr_init(proxy_t *pr)
{
  su_init();
  su_home_init(pr->pr_home);
  pr->pr_root = su_root_create(pr);
  return pr->pr_root ? 0 : 1;
}

int pr_config(proxy_t *pr, int argc, char *argv[])
{
  su_addrinfo_t *res = NULL, *ai, hints[1] = {{ 0 }};
  char *service;
  int error;
  char const *option;

  /* char const *argv0 = argv[0]; */

  while (argv[1][0] == '-') {
    option = argv[1];
    argv++, argc--;
    if (strcmp(option, "--") == 0)
      break;
    else if (strcmp(option, "-n") == 0) {
      hints->ai_flags |= AI_NUMERICHOST;
    }
    else if (strcmp(option, "-d") == 0) {
      hints->ai_socktype = SOCK_DGRAM;
    }
    else if (strcmp(option, "-s") == 0) {
      hints->ai_socktype = SOCK_STREAM;
    }
    else if (strcmp(option, "-4") == 0) {
      hints->ai_family = AF_INET;
    }
#if SU_HAVE_IN6
    else if (strcmp(option, "-6") == 0) {
      hints->ai_family = AF_INET6;
    }
#endif
  }

  if (argc < 3)
    return 2;

  if ((error = su_getaddrinfo(argv[1], argv[2], hints, &res))) {
    fprintf(stderr, "getaddrinfo: %s:%s: %s\n",
	    argv[1], argv[2], su_gai_strerror(error));
    return 1;
  }

    pr->pr_addrinfo = res;

  if (argv[3])
    service = argv[3];
  else
    service = argv[2];

  hints->ai_flags |= AI_PASSIVE;

  if ((error = su_getaddrinfo(NULL, service, hints, &res))) {
    fprintf(stderr, "getaddrinfo: %s: %s\n", service, su_gai_strerror(error));
    return 1;
  }

  for (ai = res;
       ai;
       ai = ai->ai_next) {
    forwarder_create_listener(pr, ai);
  }

  su_freeaddrinfo(res);

  if (!pr->pr_forwarders) {
    fprintf(stderr, "%s:%s: %s\n", argv[1], argv[2], "unable to forward");
    return 1;
  }

  return 0;
}

void pr_deinit(proxy_t *pr)
{
  if (pr->pr_addrinfo)
    su_freeaddrinfo(pr->pr_addrinfo), pr->pr_addrinfo = NULL;

  if (pr->pr_root)
    su_root_destroy(pr->pr_root), pr->pr_root = NULL;

  su_home_deinit(pr->pr_home);

  su_deinit();
}

static int pr_run(proxy_t *pr)
{
  su_root_run(pr->pr_root);

  return 0;
}

forwarder_t *forwarder_create(proxy_t *pr)
{
  forwarder_t *f;

  assert(pr);

  f = su_zalloc(pr->pr_home, sizeof (*f));

  if (f) {
    f->f_socket = INVALID_SOCKET;
    su_wait_init(f->f_wait);
    su_wait_init(f->f_wait + 1);
    f->f_pr = pr;
    if ((f->f_next = pr->pr_forwarders))
      f->f_next->f_prev = &f->f_next;
    f->f_prev = &pr->pr_forwarders;
    pr->pr_forwarders = f;
  }

  return f;
}

void forwarder_destroy(forwarder_t *f)
{
  if (f) {
    forwarder_deinit(f);

    if (f->f_peer) {
      f->f_peer->f_peer = NULL;
      forwarder_destroy(f->f_peer);
      f->f_peer = NULL;
    }
    assert(f->f_prev);

    if ((*f->f_prev = f->f_next))
      f->f_next->f_prev = f->f_prev;

    su_free(f->f_pr->pr_home, f);
  }
}

void forwarder_deinit(forwarder_t *f)
{
  su_root_unregister(f->f_pr->pr_root, f->f_wait, NULL, f);
  su_wait_destroy(f->f_wait);
  su_root_unregister(f->f_pr->pr_root, f->f_wait + 1, NULL, f);
  su_wait_destroy(f->f_wait + 1);
  if (f->f_socket != INVALID_SOCKET)
    su_close(f->f_socket), f->f_socket = INVALID_SOCKET;
  if (f->f_buf)
    su_free(f->f_pr->pr_home, f->f_buf), f->f_buf = NULL;
}

static forwarder_t *forwarder_create_listener(proxy_t *pr, su_addrinfo_t *ai)
{
  forwarder_t *f;
  su_socket_t s;

  if (ai->ai_socktype != SOCK_STREAM &&
      ai->ai_socktype != SOCK_DGRAM)
    return NULL;

  f = forwarder_create(pr);

  if (f) {
    s = su_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (s != INVALID_SOCKET) {
      f->f_socket = s;
      su_setblocking(s, 0);
      su_setreuseaddr(s, 1);
      if (bind(s, ai->ai_addr, ai->ai_addrlen) >= 0) {
	if (ai->ai_socktype == SOCK_STREAM ?
	    forwarder_init_stream(f) >= 0 :
	    forwarder_init_dgram(f) >= 0)
	  return f;
      }
      else {
	SU_DEBUG_1(("%s: bind: %s\n", __func__, su_strerror(su_errno())));
      }
    }
  }

  forwarder_destroy(f);

  return NULL;
}

int forwarder_init_stream(forwarder_t *f)
{
  if (listen(f->f_socket, SOMAXCONN) < 0)
    return SOCKET_ERROR;

  if (su_wait_create(f->f_wait, f->f_socket, SU_WAIT_ACCEPT) < 0)
    return SOCKET_ERROR;

  if (su_root_register(f->f_pr->pr_root, f->f_wait,
		       forwarder_accept, f, 0) < 0)
    return SOCKET_ERROR;

  return 0;
}

int forwarder_init_dgram(forwarder_t *f)
{
  /* Unimplemented */
  return SOCKET_ERROR;
}

/** Accept a connection. */
int forwarder_accept(proxy_t *pr, su_wait_t *w, forwarder_t *f0)
{
  forwarder_t *f;
  su_sockaddr_t *su;
  socklen_t  sulen;
  int events;

  events = su_wait_events(w, f0->f_socket);

  f = forwarder_create(pr);

  if (f) {
    su = f->f_dest;
    sulen = sizeof(f->f_dest);
    f->f_socket = accept(f0->f_socket, &su->su_sa, &sulen);
    f->f_upstream = 1;
    if (f->f_socket != INVALID_SOCKET) {
      char buf[SU_ADDRSIZE];

      SU_DEBUG_3(("accept: connection from %s:%u\n",
		  su_inet_ntop(su->su_family, SU_ADDR(su), buf, sizeof(buf)),
		  ntohs(su->su_port)));

      if (!su_wait_create(f->f_wait, f->f_socket, SU_WAIT_IN) &&
	  !su_wait_create(f->f_wait + 1, f->f_socket, SU_WAIT_OUT)) {
	if (forwarder_stream_peer(pr, f) != SOCKET_ERROR) {
	  /* success */
	  return 0;
	}
      }
      else {
	SU_DEBUG_1(("%s: cannot create wait objects\n", __func__));
      }
    }
  }

  forwarder_destroy(f);

  return 0;
}

int forwarder_stream_peer(proxy_t *pr, forwarder_t *f_peer)
{
  forwarder_t *f;
  su_addrinfo_t *ai;

  assert(f_peer);

  f = forwarder_create(pr);
  if (!f) {
    SU_DEBUG_1(("%s: cannot allocate peer\n", __func__));
    goto error;
  }

  for (ai = pr->pr_addrinfo; ai; ai = ai->ai_next) {
    if (ai->ai_socktype == SOCK_STREAM)
      break;
  }

  if (!ai) {
    SU_DEBUG_1(("%s: no matching destination\n", __func__));
    goto error;
  }

  f->f_socket = su_socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
  if (f->f_socket == INVALID_SOCKET) {
    SU_DEBUG_1(("%s: socket: %s\n", __func__, su_strerror(su_errno())));
    goto error;
  }

  if (su_wait_create(f->f_wait, f->f_socket, SU_WAIT_IN) ||
      su_wait_create(f->f_wait + 1, f->f_socket, SU_WAIT_OUT)) {
    SU_DEBUG_1(("%s: cannot create wait objects\n", __func__));
    goto error;
  }

  /* Asynchronous connect */
  su_setblocking(f->f_socket, 0);
  su_seterrno(0);
  connect(f->f_socket, ai->ai_addr, ai->ai_addrlen);
  memcpy(f->f_dest, ai->ai_addr, ai->ai_addrlen);

  if (su_errno() != EINPROGRESS) {
    SU_DEBUG_1(("%s: connect: %s\n", __func__, su_strerror(su_errno())));
    goto error;
  }

  if (su_root_register(pr->pr_root, f->f_wait + 1,
		       forwarder_connected, f, 0) == -1) {
    SU_DEBUG_1(("%s: cannot register\n", __func__));
    goto error;
  }

  f->f_peer = f_peer;
  f_peer->f_peer = f;
  return 0;

 error:
  forwarder_destroy(f);
  return SOCKET_ERROR;
}

/** Connection is complete. */
int forwarder_connected(proxy_t *pr, su_wait_t *w, forwarder_t *f)
{
  int events, error;
  forwarder_t *f_peer;

  events = su_wait_events(w, f->f_socket);

  error = su_soerror(f->f_socket);

  if (error) {
    SU_DEBUG_1(("connect: %s\n", su_strerror(error)));
    forwarder_destroy(f);
    return 0;
  }

  su_root_unregister(pr->pr_root, f->f_wait + 1, forwarder_connected, f);

  /* Wait for data, forward it to peer */
  assert(f->f_peer);
  f_peer = f->f_peer;
  su_root_register(pr->pr_root, f->f_wait, forwarder_recv, f, 0);
  su_root_register(pr->pr_root, f_peer->f_wait, forwarder_recv, f_peer, 0);

  return 0;
}

/** Receive data, forward it to peer */
int forwarder_recv(proxy_t *pr, su_wait_t *w, forwarder_t *f)
{
  buffer_t b[1];
  int n, events;

  events = su_wait_events(w, f->f_socket);

  n = recv(f->f_socket, b->b_data, sizeof(b->b_data), 0);

  if (n > 0) {
    b->b_sent = 0; b->b_used = n;
    if (f->f_peer->f_buf) {
      forwarder_append(f, b);
      return 0;
    }
    if (forwarder_send(pr, f->f_peer, b) >= 0) {
      if (b->b_sent < b->b_used) {
	su_root_unregister(pr->pr_root, w, forwarder_recv, f);
	su_root_register(pr->pr_root, f->f_peer->f_wait + 1,
			 forwarder_empty, f->f_peer, 0);
	forwarder_append(f, b);
      }
      return 0;
    }
    else {
      /* Error when sending */
    }
  }
  if (n < 0) {
    int error = su_errno();
    SU_DEBUG_1(("recv: %s\n", su_strerror(error)));

    if (error == EINTR || error == EAGAIN || error == EWOULDBLOCK) {
      return 0;
    }
    /* XXX */
    forwarder_destroy(f);
  }

  /* shutdown */
  forwarder_shutdown(f);

  return 0;
}

int forwarder_send(proxy_t *pr, forwarder_t *f, buffer_t *b)
{
  int n, error;

  do {
    n = send(f->f_socket, b->b_data + b->b_sent, b->b_used - b->b_sent, 0);

    if (n < 0) {
      error = su_errno();
      if (error == EINTR)
	continue;
      SU_DEBUG_1(("send: %s\n", su_strerror(error)));
      if (error != EAGAIN && error != EWOULDBLOCK)
	return -error;
    }
    else {
      f->f_sent += n;
    }
  }
  while (n > 0 && (b->b_sent += n) < b->b_used);

  return b->b_used - b->b_sent;
}

int forwarder_append(forwarder_t *f, buffer_t *b0)
{
  buffer_t *b, **bb;
  int unsent;

  /* Find last buffer */
  for (bb = &f->f_buf; *bb; bb = &(*bb)->b_next)
    ;

  unsent = b0->b_used - b0->b_sent;
  assert(unsent > 0);

  b = su_alloc(f->f_pr->pr_home, offsetof(buffer_t, b_data[unsent]));
  if (b) {
    *bb = b;
    b->b_next = NULL;
    b->b_prev = bb;
    b->b_used = unsent;
    b->b_sent = 0;
    memcpy(b->b_data, b0->b_data + b->b_sent, unsent);
  }

  return b ? 0 : -1;
}

/** Empty forwarder buffers */
int forwarder_empty(proxy_t *pr, su_wait_t *w, forwarder_t *f)
{
  buffer_t *b;
  int n, events;

  events = su_wait_events(w, f->f_socket);

  while ((b = f->f_buf)) {
    n = forwarder_send(f->f_pr, f, b);
    if (n == 0) {
      if ((f->f_buf = b->b_next))
	b->b_next->b_prev = &f->f_buf;
      su_free(f->f_pr->pr_home, b);
      continue;
    }
    else if (n < 0) {
      /* XXX */
    }
    break;
  }

  if (!f->f_buf) {
    forwarder_t *f_peer = f->f_peer;

    su_root_unregister(pr->pr_root, w, forwarder_empty, f);

    if (!f->f_shutdown) {
      /* Buffer is empty - start receiving */
      su_root_register(pr->pr_root, f_peer->f_wait, forwarder_recv, f_peer, 0);
    }
    else {
      if (shutdown(f->f_socket, 1) < 0) {
	SU_DEBUG_1(("shutdown(1): %s\n", su_strerror(su_errno())));
      }
      if (f_peer->f_shutdown) {
	forwarder_close(f);
      }
    }
  }

  return 0;
}

int forwarder_shutdown(forwarder_t *f)
{
  forwarder_t *f_peer = f->f_peer;
  su_sockaddr_t *su = f->f_dest;
  char buf[SU_ADDRSIZE];

  SU_DEBUG_3(("forwarder_shutdown: shutdown from %s:%u\n",
	      su_inet_ntop(su->su_family, SU_ADDR(su), buf, sizeof(buf)),
	      ntohs(su->su_port)));

  if (su_root_unregister(f->f_pr->pr_root, f->f_wait, forwarder_recv, f) < 0) {
    SU_DEBUG_1(("%s: su_root_unregister failed\n", __func__));
  }

  if (shutdown(f->f_socket, 0) < 0) {
    SU_DEBUG_1(("shutdown(0): %s\n", su_strerror(su_errno())));
  }
  f_peer->f_shutdown = 1;
  if (!f_peer->f_buf) {
    if (shutdown(f_peer->f_socket, 1) < 0) {
      SU_DEBUG_1(("shutdown(1): %s\n", su_strerror(su_errno())));
    }
    if (f->f_shutdown) {
      forwarder_close(f);
    }
  }
  return 0;
}

/** Close a peer pair */
void forwarder_close(forwarder_t *f)
{
  su_sockaddr_t *su1, *su2;
  char const *d1, *d2;
  char buf1[SU_ADDRSIZE], buf2[SU_ADDRSIZE];

  if (f->f_upstream)
    su1 = f->f_dest, su2 = f->f_peer->f_dest, d1 = "up", d2 = "down";
  else
    su2 = f->f_dest, su1 = f->f_peer->f_dest, d2 = "up", d1 = "down";

  SU_DEBUG_3(("forwarder_close: connection from %s:%u to %s:%d\n",
	      su_inet_ntop(su1->su_family, SU_ADDR(su1), buf1, sizeof(buf1)),
	      ntohs(su1->su_port),
	      su_inet_ntop(su2->su_family, SU_ADDR(su2), buf2, sizeof(buf2)),
	      ntohs(su2->su_port)));

  forwarder_destroy(f);
}
