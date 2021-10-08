/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005,2006 Nokia Corporation.
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

/**
 * @brief Test program for su-glib timers
 *
 * Based on torture_su_timer.c of libsofia-sip-ua.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <first.surname@nokia.com>
 *
 * @internal
 *
 * @date Created: Fri Oct 19 08:53:55 2001 pessi
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>

#include <assert.h>

struct tester;

#define SU_ROOT_MAGIC_T struct tester
#define SU_INTERNAL_P   su_root_t *
#define SU_TIMER_ARG_T  struct timing

#include "sofia-sip/su.h"
#include "sofia-sip/su_wait.h"
#include "sofia-sip/su_log.h"

#include <sofia-sip/su_glib.h>

struct timing
{
  int       t_run;
  int       t_times;
  su_time_t t_prev;
};

struct tester
{
  su_root_t *root;
  su_timer_t *t, *t1;
  unsigned times;
  void *sentinel;
};

void
print_stamp(struct tester *x, su_timer_t *t, struct timing *ti)
{
  su_time_t now = su_now(), prev = ti->t_prev;

  ti->t_prev = now;

  printf("timer interval %f\n", 1000 * su_time_diff(now, prev));

  if (!ti->t_run)
    su_timer_set(t, print_stamp, ti);

  if (++ti->t_times >= 10)
    su_timer_reset(t);
}

void
print_X(struct tester *x, su_timer_t *t1, struct timing *ti)
{
  su_timer_set(t1, print_X, ti);
  putchar('X'); fflush(stdout);
}

su_msg_r intr_msg = SU_MSG_R_INIT;

static RETSIGTYPE intr_handler(int signum)
{
  su_msg_send(intr_msg);
}

static void test_break(struct tester *tester, su_msg_r msg, su_msg_arg_t *arg)
{
  su_root_break(tester->root);
}

void
end_test(struct tester *tester, su_timer_t *t, struct timing *ti)
{
  printf("ending test\n");
  su_timer_destroy(t);
  su_timer_reset(tester->t);
  su_timer_reset(tester->t1);
  su_root_break(tester->root);
}

void
increment(struct tester *tester, su_timer_t *t, struct timing *ti)
{
  tester->times++;

  if ((void *)ti == (void*)tester->sentinel)
    su_root_break(tester->root);
}

void
usage(char const *name)
{
  fprintf(stderr, "usage: %s [-1r] [-Nnum] [interval]\n", name);
  exit(1);
}

/*
 * test su_timer functionality:
 *
 * Create a timer, executing print_stamp() in every 20 ms
 */
int main(int argc, char *argv[])
{
  su_root_t *root;
  su_timer_t *t, *t1, *t_end;
  su_timer_t **timers;
  su_duration_t interval = 60;
  char *argv0 = argv[0];
  char *s;
  int use_t1 = 0;
  su_time_t now, started;
  intptr_t i, N = 500;
  GSource *source;

  struct timing timing[1] = {{ 0 }};
  struct tester tester[1] = {{ 0 }};

  while (argv[1] && argv[1][0] == '-') {
    char *o = argv[1] + 1;
    while (*o) {
      if (*o == '1')
	o++, use_t1 = 1;
      else if (*o == 'r')
	o++, timing->t_run = 1;
      else if (*o == 'N') {
	if (o[1])
	  N = strtoul(o + 1, &o, 0);
	else if (argv[2])
	  N = strtoul(argv++[2], &o, 0);
	break;
      }
      else
	break;

    }
    if (*o)
      usage(argv0);
    argv++;
  }

  if (argv[1]) {
    interval = strtoul(argv[1], &s, 10);

    if (interval == 0 || s == argv[1])
      usage(argv0);
  }

  su_init(); atexit(su_deinit);

  tester->root = root = su_glib_root_create(tester);

  source = su_root_gsource(tester->root);
  g_source_attach(source, NULL /*g_main_context_default ()*/);

  su_msg_create(intr_msg,
		su_root_task(root),
		su_root_task(root),
		test_break, 0);

  signal(SIGINT, intr_handler);
#if HAVE_SIGPIPE
  signal(SIGPIPE, intr_handler);
  signal(SIGQUIT, intr_handler);
  signal(SIGHUP, intr_handler);
#endif

  t = su_timer_create(su_root_task(root), interval);
  t1 = su_timer_create(su_root_task(root), 1);
  t_end = su_timer_create(su_root_task(root), 20 * interval);

  if (t == NULL || t1 == NULL || t_end == NULL)
    su_perror("su_timer_create"), exit(1);

  tester->t = t, tester->t1 = t1;

  timing->t_prev = su_now();

  if (timing->t_run)
    su_timer_run(t, print_stamp, timing);
  else
    su_timer_set(t, print_stamp, timing);

  if (use_t1)
    su_timer_set(t1, print_X, NULL);

  su_timer_set(t_end, end_test, NULL);

  su_root_run(root);

  su_msg_destroy(intr_msg);

  su_timer_destroy(t);
  su_timer_destroy(t1);

  if (timing->t_times != 10) {
    fprintf(stderr, "%s: t expired %d times (expecting 10)\n",
	    argv0, timing->t_times);
    return 1;
  }

  /* Insert timers in order */
  timers = calloc(N, sizeof *timers);
  if (!timers) { perror("calloc"); exit(1); }

  now = started = su_now();

  for (i = 0; i < N; i++) {
    t = su_timer_create(su_root_task(root), 1000);
    if (!t) { perror("su_timer_create"); exit(1); }
    if (++now.tv_usec == 0) ++now.tv_sec;
    su_timer_set_at(t, increment, (void *)i, now);
    timers[i] = t;
  }

  tester->sentinel = (void*)(i - 1);

  su_root_run(root);

  printf("Processing %u timers took %f millisec (%f expected)\n",
	 (unsigned)i, su_time_diff(su_now(), started) * 1000, (double)i / 1000);

  for (i = 0; i < N; i++) {
    su_timer_destroy(timers[i]);
  }

  su_root_destroy(root);

  su_deinit();

  return 0;
}
