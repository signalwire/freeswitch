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

/**@internal
 * @file torture_su_port.c
 * @brief Test su_poll_port interface
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @date Created: Wed Mar 10 17:05:23 2004 ppessi
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>

struct su_root_magic_s;

#define SU_ROOT_MAGIC_T struct su_root_magic_s

#include "su_poll_port.c"

#undef HAVE_EPOLL
#define HAVE_EPOLL 0

#if HAVE_FUNC
#elif HAVE_FUNCTION
#define __func__ __FUNCTION__
#else
static char const __func__[] = "torture_su_port";
#endif

int tstflags;

#define TSTFLAGS tstflags

#include <sofia-sip/tstdef.h>

char const *name = "torture_su_port";

#if HAVE_OPEN_C
int const N0 = SU_MBOX_SIZE > 0, N = 63, I = 64;
#else
int const N0 = SU_MBOX_SIZE > 0, N = 128, I = 129;
#endif

int test_sup_indices(su_port_t const *port)
{
  int i, n;
  int *indices = port->sup_indices;
  int *reverses = port->sup_reverses;
  int N = port->sup_size_waits;

  if (indices == NULL)
    return N == 0 && port->sup_n_waits == 0;

  for (i = 0, n = 0; i < N; i++) {
    if (reverses[i] > 0) {
      if (indices[reverses[i]] != i)
	return 0;
      n++;
    }
  }

  if (n != port->sup_n_waits)
    return 0;

  n = 0;

  for (i = 1; i <= N; i++) {
    if (indices[i] >= 0) {
      if (reverses[indices[i]] != i)
	return 0;
      n++;
    }
  }

  if (n != port->sup_n_waits)
    return 0;

  for (i = indices[0]; -i <= N; i = indices[-i]) {
    if (i >= 0)
      return 0;
    n++;
  }

  return n == port->sup_size_waits;
}

struct su_root_magic_s
{
  int error, *sockets, *regs, *wakeups;
};

static int callback(su_root_magic_t *magic,
		    su_wait_t *w,
		    su_wakeup_arg_t *arg)
{
  intptr_t i = (intptr_t)arg;

  assert(magic);

  if (i <= 0 || i > I)
    return ++magic->error;

  su_wait_events(w, magic->sockets[i]);

  magic->wakeups[i]++;

#if HAVE_POLL || HAVE_SELECT
  if (w->fd != magic->sockets[i])
    return ++magic->error;
#endif

  return 0;
}

int test_wakeup(su_port_t *port, su_root_magic_t *magic)
{
  int i;

  for (i = N0; i < N; i++) {
    su_sockaddr_t su[1]; socklen_t sulen = sizeof su;
    int n, woken = magic->wakeups[i];
    char buf[1];
    if (magic->regs[i] == 0)
      continue;

    if (getsockname(magic->sockets[i], &su->su_sa, &sulen) < 0)
      su_perror("getsockname"), exit(1);
    if (su_sendto(magic->sockets[1], "X", 1, 0, su, sulen) < 0)
      su_perror("su_sendto"), exit(1);
    n = su_poll_port_wait_events(port, 100);

    if (n != 1)
      return 1;
    if (magic->error)
      return 2;
    if (magic->wakeups[i] != woken + 1)
      return 3;
    if (recv(magic->sockets[i], buf, 1, 0) != 1)
      return 4;
  }

  return 0;
}

int test_register(void)
{
  su_port_t *port;
  su_sockaddr_t su[1];
  intptr_t i;
  int sockets[256] = { 0 };
  int reg[256] = { 0 };
  int wakeups[256] = { 0 };
  int prioritized;
  su_wait_t wait[256];
  su_root_magic_t magic[1] = {{ 0 }};
  su_root_t root[1] = {{ sizeof root }};

  BEGIN();

  root->sur_magic = magic;

  memset(su, 0, sizeof su);
  su->su_len = sizeof su->su_sin;
  su->su_family = AF_INET;
  su->su_sin.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */

  memset(wait, 0, sizeof wait);

  memset(magic->sockets = sockets, -1, sizeof sockets);
  memset(magic->regs = reg, 0, sizeof reg);
  memset(magic->wakeups = wakeups, 0, sizeof wakeups);

  su_root_size_hint = 16;

  TEST_1(port = su_poll_port_create());
  TEST(su_home_threadsafe(su_port_home(port)), 0);
  /* Before 1.12.4 su_port_create() had reference count 0 after creation */
  /* su_port_incref(port, "test_register"); */

  TEST_1(test_sup_indices(port));

  for (i = N0; i < N; i++) {
    sockets[i] = su_socket(AF_INET, SOCK_DGRAM, 0); TEST_1(sockets[i] != -1);

    if (bind(sockets[i], &su->su_sa, sizeof su->su_sin) != 0)
      perror("bind"), assert(0);

    TEST(su_wait_create(wait + i, sockets[i], SU_WAIT_IN), 0);

    reg[i] = su_port_register(port, root, wait + i, callback, (void*)i, 0);

    TEST_1(reg[i] > 0);
  }

  TEST(port->sup_indices[0], -I);
  TEST_1(test_sup_indices(port));
  TEST(test_wakeup(port, magic), 0);

  for (i = 1; i < N; i += 2) {
    TEST(su_port_deregister(port, reg[i]), reg[i]);
    TEST_1(test_sup_indices(port));
  }

  TEST_1(test_sup_indices(port));

  prioritized = 0;

  for (i = N - 1; i >= N0; i -= 2) {
    TEST(su_wait_create(wait + i, sockets[i], SU_WAIT_IN), 0);
    reg[i] = su_port_register(port, root, wait + i, callback, (void *)i, 1);
    TEST_1(reg[i] > 0);
    prioritized++;		/* Count number of prioritized registrations */
#if HAVE_EPOLL
    /* With epoll we do not bother to prioritize the wait list */
    if (port->sup_epoll != -1) {
      int N = port->sup_n_waits;
      TEST_M(wait + i, port->sup_waits + N - 1, sizeof wait[0]);
    }
    else
#endif
    TEST_M(wait + i, port->sup_waits, sizeof wait[0]);
  }

  TEST(port->sup_indices[0], -I);

#if 0
#if HAVE_EPOLL
  /* With epoll we do not bother to prioritize the wait list */
  if (port->sup_epoll != -1) {
    TEST_M(wait + 15, port->sup_waits + 8, sizeof wait[0]);
    TEST_M(wait + 13, port->sup_waits + 9, sizeof wait[0]);
    TEST_M(wait + 11, port->sup_waits + 10, sizeof wait[0]);
    TEST_M(wait + 9, port->sup_waits + 11, sizeof wait[0]);
    TEST_M(wait + 7, port->sup_waits + 12, sizeof wait[0]);
    TEST_M(wait + 5, port->sup_waits + 13, sizeof wait[0]);
    TEST_M(wait + 3, port->sup_waits + 14, sizeof wait[0]);
    TEST_M(wait + 1, port->sup_waits + 15, sizeof wait[0]);
  }
  else
#endif
  {
  TEST_M(wait + 15, port->sup_waits + 7, sizeof wait[0]);
  TEST_M(wait + 13, port->sup_waits + 6, sizeof wait[0]);
  TEST_M(wait + 11, port->sup_waits + 5, sizeof wait[0]);
  TEST_M(wait + 9, port->sup_waits + 4, sizeof wait[0]);
  TEST_M(wait + 7, port->sup_waits + 3, sizeof wait[0]);
  TEST_M(wait + 5, port->sup_waits + 2, sizeof wait[0]);
  TEST_M(wait + 3, port->sup_waits + 1, sizeof wait[0]);
  TEST_M(wait + 1, port->sup_waits + 0, sizeof wait[0]);
  }
#endif

  TEST_1(test_sup_indices(port));
  TEST(test_wakeup(port, magic), 0);

  for (i = 1; i <= 8; i++) {
    TEST(su_port_deregister(port, reg[i]), reg[i]); reg[i] = 0;
    if (i % 2 == (N - 1) % 2)
      prioritized--;		/* Count number of prioritized registrations */
  }

#if HAVE_EPOLL
  /* With epoll we do not bother to prioritize the wait list */
  if (port->sup_epoll == -1)
#endif
    TEST(port->sup_pri_offset, prioritized);

  TEST_1(test_sup_indices(port));

  TEST(su_port_deregister(port, 0), -1);
  TEST(su_port_deregister(port, -1), -1);
  TEST(su_port_deregister(port, 130), -1);

  TEST_1(test_sup_indices(port));

  for (i = 1; i <= 8; i++) {
    TEST(su_port_deregister(port, reg[i]), -1);
  }

  TEST_VOID(su_port_decref(port, __func__));

  END();
}

void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: %s [-v] [-a]\n",
	  name);
  exit(exitcode);
}

int main(int argc, char *argv[])
{
  int retval = 0;
  int i;

#if SU_HAVE_WINSOCK
  if (N > SU_WAIT_MAX)
    N = SU_WAIT_MAX;
  if (I - 1 >= SU_WAIT_MAX)
    I = (unsigned)SU_WAIT_MAX + 1;
#endif

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      tstflags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      tstflags |= tst_abort;
    else
      usage(1);
  }

#if HAVE_OPEN_C
  tstflags |= tst_verbatim;
#endif

  su_init();

  retval |= test_register(); fflush(stdout);

  su_deinit();

  return retval;
}
