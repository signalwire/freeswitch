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

/**@ingroup su_root_ex
 * @CFILE su_perf.c
 *
 * Performance test for su message passing
 *
 * @internal
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Mar 18 19:40:51 1999 pessi
 */

#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

struct perf;
#define SU_ROOT_MAGIC_T struct perf
#define SU_MSG_ARG_T    struct message

#include "sofia-sip/su.h"
#include "sofia-sip/su_wait.h"

struct perf {
  enum { PINGER = 1, PONGER = 2 } const sort;
  char const *const name;
  unsigned          running : 1;
  unsigned          target;
  unsigned          n;
  su_root_t        *root;
  struct sockaddr_in sin;
};

struct message {
  su_time_t started;
};

void
do_exit(struct perf *x, su_msg_r msg, struct message *m)
{
  su_root_break(su_task_root(su_msg_to(msg)));
}

int
do_init(su_root_t *root, struct perf *p)
{
  p->root = root;
  return 0;
}

void
do_print(struct perf *p, su_msg_r msg, struct message *m)
{
  su_time_t now = su_now();
  double dur = su_time_diff(now, m->started);

  printf("su_perf: %g message exchanges per second"
	 " (%d message exchanges in %g seconds)\n",
	 (double)p->n / dur, p->n, dur);
  su_msg_create(msg, su_root_parent(p->root), su_root_task(p->root),
		do_exit, sizeof(*m));
  *su_msg_data(msg) = *m;
  su_msg_send(msg);
}

void
do_ping(struct perf *p, su_msg_r msg, struct message *m)
{
  if (p->sort == PINGER) {
    p->n++;

    if (p->n % 100 == 0) {
      if (su_duration(su_now(), m->started) > p->target) {
	do_print(p, msg, m);
	return;
      }
    }
  }

  su_msg_reply(msg, msg, do_ping, sizeof(*m));
  *su_msg_data(msg) = *m;
  su_msg_send(msg);
}

void
do_destroy(su_root_t *t, struct perf *p)
{
  p->running = 0;
}

void usage(char *name)
{
  fprintf(stderr, "usage: %s [-1] [n]\n", name);
  exit(2);
}

/*
 * Measure how many message passes can be done in a second.
 *
 * Create a ponger and pinger, responding to incoming message do_ping
 *
 * After "target" rounds, print out elapsed time and number of messages
 * passed.
 *
 */
int main(int argc, char *argv[])
{
  su_root_t *root;
  su_clone_r ping = SU_CLONE_R_INIT, pong = SU_CLONE_R_INIT;
  su_msg_r start_msg = SU_MSG_R_INIT;

  struct perf
    pinger = { PINGER, "ping", 1, 0 },
    ponger = { PONGER, "pong", 1, 0x7fffffff};

  int have_threads = 1;
  char *argv0 = argv[0];
  char *argv1 = argv[1];

  if (argv1 && strcmp(argv1, "-1") == 0)
    have_threads = 0, argv1 = argv[2];

  if (!argv1)
    argv1 = "10000";

  if (strlen(argv1) != strspn(argv1, "0123456789"))
    usage(argv0);

  pinger.target = strtoul(argv1, NULL, 0);

  su_init(); atexit(su_deinit);

  root = su_root_create(NULL);

  su_root_threading(root, have_threads);

  if (su_clone_start(root, ping, &pinger, do_init, do_destroy) != 0)
    perror("su_clone_start"), exit(1);
  if (su_clone_start(root, pong, &ponger, do_init, do_destroy) != 0)
    perror("su_clone_start"), exit(1);

  if (su_msg_create(start_msg, su_clone_task(pong), su_clone_task(ping),
		    do_ping, sizeof(struct message)) == 0) {
    su_msg_data(start_msg)->started = su_now();
    su_msg_send(start_msg);

    su_root_run(root);
  }

#if 0
  su_clone_wait(root, ping);
  su_clone_wait(root, pong);

  while (pinger.running || ponger.running)
    su_root_step(root, 100L);
#endif

  su_root_destroy(root);

  return 0;
}
