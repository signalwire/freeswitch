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
 * @CFILE test_su.c
 *
 * @brief Test program for su wait objects, root, timers, clones, and messages.
 *
 * @internal
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Thu Mar 18 19:40:51 1999 pessi
 */

#include "config.h"

struct pinger;
#define SU_ROOT_MAGIC_T struct pinger
#define SU_INTERNAL_P   su_root_t *
#define SU_MSG_ARG_T    su_sockaddr_t

#include "sofia-sip/su.h"
#include "sofia-sip/su_wait.h"
#include "sofia-sip/su_log.h"
#include "sofia-sip/su_debug.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <assert.h>

char const name[] = "test_su";

#if HAVE_FUNC
#define enter (void)SU_DEBUG_9(("%s: %s: entering\n", name, __func__))
#define nh_enter (void)SU_DEBUG_9(("%s %s(%p): entering\n", name, __func__, nh))
#elif HAVE_FUNCTION
#define enter (void)SU_DEBUG_9(("%s: %s: entering\n", name, __FUNCTION__))
#define nh_enter (void)SU_DEBUG_9(("%s %s(%p): entering\n", name, __FUNCTION__, nh))
#define __func__ __FUNCTION__
#else
#define enter ((void)0)
#define nh_enter ((void)0)
#define __func__ "test_su"
#endif

struct pinger {
  enum { PINGER = 1, PONGER = 2 } const sort;
  char const *  name;
  unsigned      running : 1;
  unsigned      : 0;
  su_root_t    *root;
  su_socket_t   s;
  su_timer_t   *t;
  int           id;
  int           rindex;
  su_time_t     when;
  su_sockaddr_t addr;
  double        rtt_total;
  int           rtt_n;
};

short opt_family = AF_INET;
short opt_verbatim = 0;
short opt_singlethread = 0;

static su_socket_t udpsocket(void)
{
  su_socket_t s;
  su_sockaddr_t su = { 0 };
  socklen_t sulen = sizeof(su);
  char nbuf[64];

  su.su_family = opt_family;

  su_getlocalip(&su);

  s = su_socket(su.su_family, SOCK_DGRAM, 0);
  if (s == INVALID_SOCKET) {
    su_perror("udpsocket: socket");
    exit(1);
  }

  if (bind(s, &su.su_sa, su_sockaddr_size(&su)) == SOCKET_ERROR) {
    su_perror("udpsocket: bind");
    exit(1);
  }

  if (getsockname(s, &su.su_sa, &sulen) == SOCKET_ERROR) {
    su_perror("udpsocket: getsockname");
    exit(1);
  }

  if (opt_verbatim)
    printf("udpsocket: using address [%s]:%u\n",
	   su_inet_ntop(su.su_family, SU_ADDR(&su), nbuf, sizeof(nbuf)),
	   ntohs(su.su_sin.sin_port));

  return s;
}

static char *snow(su_time_t now)
{
  static char buf[24];

  su_time_print(buf, sizeof(buf), &now);

  return buf;
}

void
do_ping(struct pinger *p, su_timer_t *t, void *p0)
{
  char buf[1024];

  assert(p == su_root_magic(su_timer_root(t)));
  assert(p->sort == PINGER);

  p->when = su_now();

  snprintf(buf, sizeof(buf), "Ping %d at %s", p->id++, snow(p->when));
  if (su_sendto(p->s, buf, strlen(buf), 0,
		&p->addr, su_sockaddr_size(&p->addr)) == -1) {
    su_perror("do_ping: send");
  }

  if (opt_verbatim) {
    puts(buf);
    fflush(stdout);
  }
}

int
do_rtt(struct pinger *p, su_wait_t *w, void *p0)
{
  su_sockaddr_t su;
  struct sockaddr * const susa = &su.su_sa;
  socklen_t susize[] = { sizeof(su)};
  char buf[1024];
  char nbuf[1024];
  int n;
  su_time_t now = su_now();
  double rtt;

  assert(p0 == p);
  assert(p->sort == PINGER);

  rtt = su_time_diff(now, p->when);

  p->rtt_total += rtt, p->rtt_n++;

  su_wait_events(w, p->s);

  n = recvfrom(p->s, buf, sizeof(buf) - 1, 0, susa, susize);
  if (n < 0) {
	  su_perror("do_rtt: recvfrom");
	  return 0;
  }
  buf[n] = 0;

  if (opt_verbatim)
    printf("do_rtt: %d bytes from [%s]:%u: \"%s\", rtt = %lg ms\n",
	   n, su_inet_ntop(su.su_family, SU_ADDR(&su), nbuf, sizeof(nbuf)),
	   ntohs(su.su_sin.sin_port), buf, rtt / 1000);

  do_ping(p, p->t, NULL);

  return 0;
}

void
do_pong(struct pinger *p, su_timer_t *t, void *p0)
{
  char buf[1024];

  assert(p == su_root_magic(su_timer_root(t)));
  assert(p->sort == PONGER);

  p->id = 0;

  snprintf(buf, sizeof(buf), "Pong at %s", snow(su_now()));
  if (su_sendto(p->s, buf, strlen(buf), 0,
		&p->addr, su_sockaddr_size(&p->addr)) == -1) {
    su_perror("do_pong: send");
  }

  if (opt_verbatim) {
    puts(buf);
    fflush(stdout);
  }
}

int
do_recv(struct pinger *p, su_wait_t *w, void *p0)
{
  su_sockaddr_t su;
  socklen_t susize[] = { sizeof(su)};
  char buf[1024];
  char nbuf[1024];
  int n;
  su_time_t now = su_now();

  assert(p0 == p);
  assert(p->sort == PONGER);

  su_wait_events(w, p->s);

  n = recvfrom(p->s, buf, sizeof(buf) - 1, 0, &su.su_sa, susize);
  if (n < 0) {
	  su_perror("do_recv: recvfrom");
	  return 0;
  }
  buf[n] = 0;

  if (opt_verbatim)
    printf("do_recv: %d bytes from [%s]:%u: \"%s\" at %s\n",
	   n, su_inet_ntop(su.su_family, SU_ADDR(&su), nbuf, sizeof(nbuf)),
	   ntohs(su.su_sin.sin_port), buf, snow(now));

  fflush(stdout);

#if 0
  if (p->id)
    puts("do_recv: already a pending reply");

  if (su_timer_set(p->t, do_pong, p) < 0) {
    fprintf(stderr, "do_recv: su_timer_set() error\n");
    return 0;
  }

  p->id = 1;
#else
  do_pong(p, p->t, NULL);
#endif

  return 0;
}

void
do_exit(struct pinger *x, su_timer_t *t, void *x0)
{
  if (opt_verbatim)
    printf("do_exit at %s\n", snow(su_now()));
  su_root_break(su_timer_root(t));
}

int
do_init(su_root_t *root, struct pinger *p)
{
  su_wait_t w;
  su_socket_t s;
  long interval;
  su_timer_t *t;
  su_wakeup_f f;
  int index;
#if nomore
  su_wait_t w0;
  int index0
#endif

  enter;

  switch (p->sort) {
  case PINGER: f = do_rtt;  interval = 200; break;
  case PONGER: f = do_recv; interval = 40;  break;
  default:
    return SU_FAILURE;
  }

  p->t = t = su_timer_create(su_root_task(root), interval);
  if (t == NULL) {
    su_perror("su_timer_create");
    return SU_FAILURE;
  }

  /* Create a sockets,  */
  p->s = s = udpsocket();

#if nomore
  if (su_wait_create(&w0, s, SU_WAIT_IN) == SOCKET_ERROR) {
    su_perror("su_wait_create");
    return SU_FAILURE;
  }

  index0 = su_root_register(root, &w0, f, p, 0);
  if (index0 == SOCKET_ERROR) {
    su_perror("su_root_register");
    return SU_FAILURE;
  }
#endif

  if (su_wait_create(&w, s, SU_WAIT_IN) == SOCKET_ERROR) {
    su_perror("su_wait_create");
    return SU_FAILURE;
  }

  index = su_root_register(root, &w, f, p, 0);
  if (index == SOCKET_ERROR) {
    su_perror("su_root_register");
    return SU_FAILURE;
  }

#if nomore
  su_root_deregister(root, index0);
#endif
  p->rindex = index;

  return 0;
}

void
do_destroy(su_root_t *root, struct pinger *p)
{
  enter;

  if (opt_verbatim)
    printf("do_destroy %s at %s\n", p->name, snow(su_now()));
  su_root_deregister(root, p->rindex);
  su_timer_destroy(p->t), p->t = NULL;
  p->running = 0;
}

void
start_ping(struct pinger *p, su_msg_r msg, su_sockaddr_t *arg)
{
  enter;

  if (!p->running)
    return;

  if (opt_verbatim)
    printf("start_ping: %s\n", p->name);

  p->addr = *arg;
  p->id = 1;

  su_timer_set_interval(p->t, do_ping, p, 0);
}

void
start_pong(struct pinger *p, su_msg_r msg, su_sockaddr_t *arg)
{
  su_msg_r reply;

  enter;

  if (!p->running)
    return;

  if (opt_verbatim)
    printf("start_pong: %s\n", p->name);

  p->addr = *arg;

  if (su_msg_reply(reply, msg, start_ping, sizeof(p->addr)) == 0) {
    socklen_t sinsize[1] = { sizeof(p->addr) };
    if (getsockname(p->s, (struct sockaddr*)su_msg_data(reply), sinsize)
	== SOCKET_ERROR)
      su_perror("start_pong: getsockname()"), exit(1);
    su_msg_send(reply);
  }
  else {
    fprintf(stderr, "su_msg_reply failed!\n");
  }
}

void
init_ping(struct pinger *p, su_msg_r msg, su_sockaddr_t *arg)
{
  su_msg_r reply;

  enter;

  if (opt_verbatim)
    printf("init_ping: %s\n", p->name);

  if (su_msg_reply(reply, msg, start_pong, sizeof(p->addr)) == 0) {
    socklen_t sinsize[1] = { sizeof(p->addr) };
    if (getsockname(p->s, (struct sockaddr*)su_msg_data(reply), sinsize)
	== SOCKET_ERROR)
      su_perror("start_pong: getsockname()"), exit(1);
    su_msg_send(reply);
  }
  else {
    fprintf(stderr, "su_msg_reply failed!\n");
  }
}

void
time_test(void)
{
  su_time_t now = su_now(), then = now;
  su_duration_t t1, t2;
  su_duration_t us;

  enter;

  for (us = 0; us < 1000000; us += 300) {
    then.tv_sec = now.tv_sec;
    if ((then.tv_usec = now.tv_usec + us) >= 1000000)
      then.tv_usec -= 1000000, then.tv_sec++;
    t1 = su_duration(now, then);
    t2 = su_duration(then, now);
    assert(t1 == -t2);
  }

  if (opt_verbatim)
    printf("time_test: passed\n");
}

#if HAVE_SIGNAL
#include <unistd.h>
#include <signal.h>

#if HAVE_ALARM

static RETSIGTYPE sig_alarm(int s)
{
  enter;

  fprintf(stderr, "%s: FAIL! test timeout!\n", name);
  exit(1);
}
static char const no_alarm[] = " [--no-alarm]";
#else
static char const no_alarm[] = "";
#endif

static
RETSIGTYPE term(int n)
{
  enter;

  exit(1);
}
#else
static char const no_alarm[] = "";
#endif


void
usage(int exitcode)
{
  fprintf(stderr, "usage: %s [-6vs]%s\n", name, no_alarm);

  exit(exitcode);
}

/*
 * test su_wait functionality:
 *
 * Create a ponger, waking up do_recv() when data arrives,
 *                  then scheduling do_pong() by timer
 *
 * Create a pinger, executed from timer, scheduling do_ping(),
 *                  waking up do_rtt() when data arrives
 *
 * Create a timer, executing do_exit() after 10 seconds
 */
int main(int argc, char *argv[])
{
  su_root_t *root;
  su_clone_r ping = SU_CLONE_R_INIT, pong = SU_CLONE_R_INIT;
  su_msg_r start_msg = SU_MSG_R_INIT;
  su_timer_t *t;
#if HAVE_ALARM
  int opt_alarm = 1;
#endif

  struct pinger
    pinger = { PINGER, "ping", 1 },
    ponger = { PONGER, "pong", 1 };

  char *argv0 = argv[0];

  enter;

  while (argv[1]) {
    if (strcmp(argv[1], "-v") == 0) {
      opt_verbatim = 1;
      argv++;
    }
#if SU_HAVE_IN6
    else if (strcmp(argv[1], "-6") == 0) {
      opt_family = AF_INET6;
      argv++;
    }
#endif
    else if (strcmp(argv[1], "-s") == 0) {
      opt_singlethread = 1;
      argv++;
    }
#if HAVE_ALARM
    else if (strcmp(argv[1], "--no-alarm") == 0) {
      opt_alarm = 0;
      argv++;
    }
#endif
    else {
      usage(1);
    }
  }

  su_init(); atexit(su_deinit);

#if HAVE_SIGNAL
  signal(SIGTERM, term);

#if HAVE_ALARM
  if (opt_alarm) {
    alarm(60);
    signal(SIGALRM, sig_alarm);
  }
#endif

#endif

  time_test();

  root = su_root_create(NULL);
  if (!root) perror("su_root_create"), exit(1);

  fprintf(stdout, "test_su: testing %s port implementation\n",
	  su_root_name(root));

  su_root_threading(root, !opt_singlethread);

  if (su_clone_start(root, ping, &pinger, do_init, do_destroy) != 0)
    perror("su_clone_start"), exit(1);
  if (su_clone_start(root, pong, &ponger, do_init, do_destroy) != 0)
    perror("su_clone_start"), exit(1);

  /* Test timer, exiting after 200 milliseconds */
  t = su_timer_create(su_root_task(root), 200L);
  if (t == NULL)
    su_perror("su_timer_create"), exit(1);
  su_timer_set(t, (su_timer_f)do_exit, NULL);

  su_msg_create(start_msg, su_clone_task(ping), su_clone_task(pong),
		init_ping, 0);
  su_msg_send(start_msg);

  su_root_run(root);

  su_clone_wait(root, ping);
  su_clone_wait(root, pong);

  su_timer_destroy(t);

  if (pinger.rtt_n) {
    printf("%s executed %u pings in %g, mean rtt=%g sec\n", name,
	   pinger.rtt_n, pinger.rtt_total, pinger.rtt_total / pinger.rtt_n);
  }
  su_root_destroy(root);

  if (opt_verbatim)
    printf("%s exiting\n", argv0);

  return 0;
}
