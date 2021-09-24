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
 * @file torture_su_root.c
 *
 * Test su_root_register functionality.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * Copyright (c) 2002 Nokia Research Center.  All rights reserved.
 *
 * @date Created: Wed Jun 12 15:18:11 2002 ppessi
 */

#include "config.h"

char const *name = "torture_su_root";

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define TSTFLAGS rt->rt_flags
#include <sofia-sip/tstdef.h>

typedef struct root_test_s root_test_t;
typedef struct test_ep_s   test_ep_t;

#define SU_ROOT_MAGIC_T  root_test_t
#define SU_WAKEUP_ARG_T  test_ep_t
#define SU_MSG_ARG_T     root_test_t *

#include <sofia-sip/su_wait.h>
#include <sofia-sip/su_alloc.h>
#include <sofia-sip/su_log.h>

#if SU_HAVE_PTHREADS
#include <pthread.h>
#endif

#define ALARM_IN_SECONDS 120

struct test_ep_s {
  test_ep_t     *next, **prev, **list;
  int           i;
  int           s;
  su_wait_t     wait[1];
  int           registered;
  socklen_t     addrlen;
  su_sockaddr_t addr[1];
};

typedef struct test_ep_s   test_ep_at[1];

struct root_test_s {
  su_home_t  rt_home[1];
  int        rt_flags;

  su_root_t *rt_root;
  short      rt_family;
  int        rt_status;
  int        rt_received;
  int        rt_wakeup;

  su_clone_r rt_clone;

  unsigned   rt_msg_received;
  unsigned   rt_msg_destroyed;

  unsigned   rt_fail_init:1;
  unsigned   rt_fail_deinit:1;
  unsigned   rt_success_init:1;
  unsigned   rt_success_deinit:1;

  unsigned   rt_sent_reporter:1;
  unsigned   rt_recv_reporter:1;
  unsigned   rt_reported_reporter:1;

  unsigned   rt_executed:1;

  unsigned   rt_t1:1, rt_t2:1;

  unsigned :0;

  test_ep_at rt_ep[5];

  int rt_sockets, rt_woken;

#if SU_HAVE_PTHREADS
  struct {
    /* Used to test su_root_obtain()/su_root_release() */
    pthread_mutex_t mutex[1];
    pthread_cond_t cond[1];
    pthread_t slave;
    int done;
    pthread_mutex_t deinit[1];
  } rt_sr;
#endif
};

int test_api(root_test_t *rt)
{
  BEGIN();
  TEST_1(rt->rt_root = su_root_create(NULL));

  TEST_VOID(su_root_destroy(NULL));
  TEST_P(su_root_name(NULL), NULL);
  TEST_1(su_root_name(rt->rt_root) != NULL);
  TEST(su_root_set_magic(NULL, rt), -1);
  TEST_P(su_root_magic(rt->rt_root), NULL);
  TEST(su_root_set_magic(rt->rt_root, rt), 0);
  TEST_P(su_root_magic(rt->rt_root), rt);
  TEST_P(su_root_magic(NULL), NULL);
  TEST(su_root_register(NULL, NULL, NULL, NULL, 0), -1);
  TEST(su_root_unregister(NULL, NULL, NULL, NULL), -1);
  TEST(su_root_deregister(NULL, 0), -1);
  TEST(su_root_deregister(rt->rt_root, 0), -1);
  TEST(su_root_deregister(rt->rt_root, -1), -1);
  TEST(su_root_eventmask(NULL, 0, -1, -1), -1);
  TEST((long int)su_root_step(NULL, 0), -1L);
  TEST((long int)su_root_sleep(NULL, 0), -1L);
  TEST(su_root_multishot(NULL, 0), -1);
  TEST_VOID((void)su_root_run(NULL));
  TEST_VOID((void)su_root_break(NULL));
  TEST_M(su_root_task(NULL), su_task_null, sizeof su_task_null);
  TEST_M(su_root_parent(NULL), su_task_null, sizeof su_task_null);
  TEST(su_root_add_prepoll(NULL, NULL, NULL), -1);
  TEST(su_root_remove_prepoll(NULL), -1);
  TEST_P(su_root_gsource(NULL), NULL);
  TEST_VOID((void)su_root_gsource(rt->rt_root));
  TEST(su_root_yield(NULL), -1);
  TEST(su_root_release(NULL), -1);
  TEST(su_root_obtain(NULL), -1);
  TEST(su_root_has_thread(NULL), -1);
  TEST(su_root_has_thread(rt->rt_root), 2);
  TEST(su_root_release(rt->rt_root), 0);
  TEST(su_root_has_thread(rt->rt_root), 0);
  TEST(su_root_obtain(rt->rt_root), 0);
  TEST(su_root_has_thread(rt->rt_root), 2);
  TEST_VOID((void)su_root_destroy(rt->rt_root));
  rt->rt_root = NULL;
  END();
}

#if SU_HAVE_PTHREADS

#include <pthread.h>

void *suspend_resume_test_thread(void *_rt)
{
  root_test_t *rt = _rt;

  su_init();

  pthread_mutex_lock(rt->rt_sr.mutex);
  rt->rt_root = su_root_create(rt);
  rt->rt_sr.done = 1;
  pthread_cond_signal(rt->rt_sr.cond);
  pthread_mutex_unlock(rt->rt_sr.mutex);
  su_root_release(rt->rt_root);

  pthread_mutex_lock(rt->rt_sr.deinit);
  su_root_obtain(rt->rt_root);
  su_root_destroy(rt->rt_root);
  rt->rt_root = NULL;
  pthread_mutex_unlock(rt->rt_sr.deinit);

  su_deinit();

  return NULL;
}
#endif

/** Test root initialization */
int init_test(root_test_t *rt,
	      char const *preference,
	      su_port_create_f *create,
	      su_clone_start_f *start)
{
  su_sockaddr_t su[1] = {{ 0 }};
  int i;

  BEGIN();

  su_port_prefer(create, start);

#if SU_HAVE_PTHREADS
  pthread_mutex_init(rt->rt_sr.mutex, NULL);
  pthread_cond_init(rt->rt_sr.cond, NULL);
  pthread_mutex_init(rt->rt_sr.deinit, NULL);

  pthread_mutex_lock(rt->rt_sr.deinit);

  pthread_create(&rt->rt_sr.slave, NULL, suspend_resume_test_thread, rt);

  pthread_mutex_lock(rt->rt_sr.mutex);
  while (rt->rt_sr.done == 0)
    pthread_cond_wait(rt->rt_sr.cond, rt->rt_sr.mutex);
  pthread_mutex_unlock(rt->rt_sr.mutex);

  TEST_1(rt->rt_root);
  TEST(su_root_obtain(rt->rt_root), 0);
  TEST(su_root_has_thread(rt->rt_root), 2);
#else
  TEST_1(rt->rt_root = su_root_create(rt));
#endif

  printf("%s: testing %s (%s) implementation\n",
	 name, preference, su_root_name(rt->rt_root));

  su->su_family = rt->rt_family;

  for (i = 0; i < 5; i++) {
    test_ep_t *ep = rt->rt_ep[i];
    ep->i = i;
    ep->addrlen = su_sockaddr_size(su);
    TEST_1((ep->s = su_socket(su->su_family, SOCK_DGRAM, 0)) != -1);
    TEST_1(bind(ep->s, &su->su_sa, ep->addrlen) != -1);
    TEST_1(su_wait_create(ep->wait, ep->s, SU_WAIT_IN|SU_WAIT_ERR) != -1);
    TEST_1(getsockname(ep->s, &ep->addr->su_sa, &ep->addrlen) != -1);
    if (SU_HAS_INADDR_ANY(ep->addr)) {
      su_inet_pton(su->su_family,
		   su->su_family == AF_INET ? "127.0.0.1" : "::1",
		   SU_ADDR(ep->addr));
    }
  }

  END();
}

static int deinit_test(root_test_t *rt)
{
  BEGIN();

#if SU_HAVE_PTHREADS
  TEST(su_root_has_thread(rt->rt_root), 2);
  TEST(su_root_release(rt->rt_root), 0);
  TEST(su_root_has_thread(rt->rt_root), 0);
  pthread_mutex_unlock(rt->rt_sr.deinit);
  pthread_join(rt->rt_sr.slave, NULL);
  pthread_mutex_destroy(rt->rt_sr.mutex);
  pthread_cond_destroy(rt->rt_sr.cond);
  pthread_mutex_destroy(rt->rt_sr.deinit);
#else
  TEST_VOID(su_root_destroy(rt->rt_root)); rt->rt_root = NULL;
#endif

  END();
}

int wakeup(root_test_t *rt,
	   su_wait_t *w,
	   test_ep_t *ep)
{
  char buffer[64];
  int n, error;

  su_wait_events(w, ep->s);

  n = recv(ep->s, buffer, sizeof(buffer), 0);
  error = su_errno();

  if (n < 0)
    fprintf(stderr, "%s: %s\n", "recv", su_strerror(error));

  TEST_1(n > 0);

  rt->rt_received = ep->i;

  return 0;
}

static int wakeup0(root_test_t *rt, su_wait_t *w, test_ep_t *ep)
{
  rt->rt_wakeup = 0;
  return wakeup(rt, w, ep);
}
static int wakeup1(root_test_t *rt, su_wait_t *w, test_ep_t *ep)
{
  rt->rt_wakeup = 1;
  return wakeup(rt, w, ep);
}
static int wakeup2(root_test_t *rt, su_wait_t *w, test_ep_t *ep)
{
  rt->rt_wakeup = 2;
  return wakeup(rt, w, ep);
}
static int wakeup3(root_test_t *rt, su_wait_t *w, test_ep_t *ep)
{
  rt->rt_wakeup = 3;
  return wakeup(rt, w, ep);
}
static int wakeup4(root_test_t *rt, su_wait_t *w, test_ep_t *ep)
{
  rt->rt_wakeup = 4;
  return wakeup(rt, w, ep);
}

static
su_wakeup_f wakeups[5] = { wakeup0, wakeup1, wakeup2, wakeup3, wakeup4 };

static
void test_run(root_test_t *rt)
{
  rt->rt_received = -1;

  while (rt->rt_received == -1) {
    su_root_step(rt->rt_root, 200);
  }
}

static int register_test(root_test_t *rt)
{
  int i;
  int s;
  char *msg = "foo";

  BEGIN();

  TEST_1((s = su_socket(rt->rt_family, SOCK_DGRAM, 0)) != -1);

  for (i = 0; i < 5; i++) {
    rt->rt_ep[i]->registered =
      su_root_register(rt->rt_root, rt->rt_ep[i]->wait,
		       wakeups[i], rt->rt_ep[i], 0);
    TEST(rt->rt_ep[i]->registered, i + 1 + SU_HAVE_PTHREADS);
  }

  for (i = 0; i < 5; i++) {
    test_ep_t *ep = rt->rt_ep[i];
    TEST_SIZE(su_sendto(s, msg, sizeof(msg), 0, ep->addr, ep->addrlen),
	      sizeof(msg));
    test_run(rt);
    TEST(rt->rt_received, i);
    TEST(rt->rt_wakeup, i);
  }

  for (i = 0; i < 5; i++) {
    TEST(su_root_unregister(rt->rt_root, rt->rt_ep[i]->wait,
			    wakeups[i], rt->rt_ep[i]),
	 rt->rt_ep[i]->registered);
  }


  for (i = 0; i < 5; i++) {
    rt->rt_ep[i]->registered =
      su_root_register(rt->rt_root, rt->rt_ep[i]->wait,
		       wakeups[i], rt->rt_ep[i], 1);
    TEST_1(rt->rt_ep[i]->registered > 0);
  }

  for (i = 0; i < 5; i++) {
    test_ep_t *ep = rt->rt_ep[i];
    TEST_SIZE(su_sendto(s, msg, sizeof(msg), 0, ep->addr, ep->addrlen),
	      sizeof(msg));
    test_run(rt);
    TEST(rt->rt_received, i);
    TEST(rt->rt_wakeup, i);
  }

  for (i = 0; i < 5; i++) {
    TEST(su_root_deregister(rt->rt_root, rt->rt_ep[i]->registered),
	 rt->rt_ep[i]->registered);
  }

  for (i = 0; i < 5; i++) {
    test_ep_t *ep = rt->rt_ep[i];
    TEST_1(su_wait_create(ep->wait, ep->s, SU_WAIT_IN|SU_WAIT_ERR) != -1);
    ep->registered =
      su_root_register(rt->rt_root, ep->wait,
		       wakeups[i], ep, 1);
    TEST_1(ep->registered > 0);
  }

  for (i = 0; i < 5; i++) {
    test_ep_t *ep = rt->rt_ep[i];
    TEST_SIZE(su_sendto(s, msg, sizeof(msg), 0, ep->addr, ep->addrlen),
	      sizeof(msg));
    test_run(rt);
    TEST(rt->rt_received, i);
    TEST(rt->rt_wakeup, i);
  }

  for (i = 0; i < 5; i++) {
    TEST(su_root_unregister(rt->rt_root, rt->rt_ep[i]->wait,
			    wakeups[i], rt->rt_ep[i]),
	 rt->rt_ep[i]->registered);
  }

  END();
}


int wakeup_remove(root_test_t *rt, su_wait_t *w, test_ep_t *node)
{
  char buffer[64];
  ssize_t x;
  test_ep_t *n = node->next;

  su_wait_events(w, node->s);

  x = recv(node->s, buffer, sizeof(buffer), 0);

  if (x < 0)
    fprintf(stderr, "%s: %s\n", "recv", su_strerror(su_errno()));

  if (node->prev) {		/* first run */
    *node->prev = n;

    if (n) {
      *node->prev = node->next;
      node->next->prev = node->prev;
      sendto(n->s, "foo", 3, 0, (void *)n->addr, n->addrlen);
    }

    node->next = NULL;
    node->prev = NULL;

    if (!*node->list) {
      su_root_break(rt->rt_root);
    }
  }
  else {			/* second run */
    if (++rt->rt_woken == rt->rt_sockets)
      su_root_break(rt->rt_root);
  }

  return 0;
}


int event_test(root_test_t rt[1])
{
  BEGIN();
  int i = 0, N = 2048;
  test_ep_t *n, *nodes, *list = NULL;
  su_sockaddr_t su[1];
  socklen_t sulen;

  TEST_1(nodes = calloc(N, sizeof *nodes));

  memset(su, 0, sulen = sizeof su->su_sin);
  su->su_len = sizeof su->su_sin;
  su->su_family = AF_INET;
  su->su_sin.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */

  for (i = 0; i < N; i++) {
    n = nodes + i;
    n->s = su_socket(AF_INET, SOCK_DGRAM, 0);

    if (n->s == INVALID_SOCKET)
      break;

    n->addrlen = sizeof n->addr;

    n->addr->su_len = sizeof n->addr;

    if (bind(n->s, (void *)su, sulen) < 0) {
      su_perror("bind()");
      su_close(n->s);
      break;
    }

    if (getsockname(n->s, (void *)n->addr, &n->addrlen)) {
      su_perror("getsockname()");
      su_close(n->s);
      break;
    }

    if (su_wait_create(n->wait, n->s, SU_WAIT_IN)) {
      su_perror("su_wait_create()");
      su_close(n->s);
      break;
    }

    n->registered = su_root_register(rt->rt_root, n->wait, wakeup_remove, n, 0);
    if (n->registered < 0) {
      su_wait_destroy(n->wait);
      su_close(n->s);
      break;
    }

    n->list = &list, n->prev = &list;
    if ((n->next = list))
      n->next->prev = &n->next;
    list = n;
  }

  TEST_1(i >= 1);

  N = i;

  /* Wake up socket at a time */
  n = list; sendto(n->s, "foo", 3, 0, (void *)n->addr, n->addrlen);

  su_root_run(rt->rt_root);

  for (i = 0; i < N; i++) {
    n = nodes + i;
    TEST_1(n->prev == NULL);
    sendto(n->s, "bar", 3, 0, (void *)n->addr, n->addrlen);
  }

  rt->rt_sockets = N;

  /* Wake up all sockets */
  su_root_run(rt->rt_root);

  for (i = 0; i < N; i++) {
    n = nodes + i;
    su_root_deregister(rt->rt_root, n->registered);
    TEST_1(su_close(n->s) == 0);
  }

  free(nodes);

  END();
}

int fail_init(su_root_t *root, root_test_t *rt)
{
  rt->rt_fail_init = 1;
  return -1;
}

void fail_deinit(su_root_t *root, root_test_t *rt)
{
  rt->rt_fail_deinit = 1;
}

int success_init(su_root_t *root, root_test_t *rt)
{
  rt->rt_success_init = 1;
  return 0;
}

void success_deinit(su_root_t *root, root_test_t *rt)
{
  rt->rt_success_deinit = 1;
}

void receive_a_reporter(root_test_t *rt,
			su_msg_r msg,
			su_msg_arg_t *arg)
{
  rt->rt_recv_reporter = 1;
}

void receive_recv_report(root_test_t *rt,
			 su_msg_r msg,
			 su_msg_arg_t *arg)
{
  rt->rt_reported_reporter = 1;
}

void send_a_reporter_msg(root_test_t *rt,
			 su_msg_r msg,
			 su_msg_arg_t *arg)
{
  su_msg_r m = SU_MSG_R_INIT;

  if (su_msg_create(m,
		    su_msg_from(msg),
		    su_msg_to(msg),
		    receive_a_reporter,
		    0) == 0 &&
      su_msg_report(m, receive_recv_report) == 0 &&
      su_msg_send(m) == 0)
    rt->rt_sent_reporter = 1;
}

static void expire1(root_test_t *rt, su_timer_t *t, su_timer_arg_t *arg)
{
  (void)arg;
  rt->rt_t1 = 1;
}

static void expire2(root_test_t *rt, su_timer_t *t, su_timer_arg_t *arg)
{
  (void)arg;
  rt->rt_t2 = 1;
}

int timer_test(root_test_t rt[1])
{
  BEGIN();

  su_timer_t *t1, *t2;
  su_duration_t defer;

  TEST_1(t1 = su_timer_create(su_root_task(rt->rt_root), 100));
  TEST_1(t2 = su_timer_create(su_root_task(rt->rt_root), 110));

  rt->rt_t1 = rt->rt_t2 = 0;

  TEST_1(su_root_step(rt->rt_root, 0) == SU_WAIT_FOREVER);

  TEST_1(su_root_set_max_defer(rt->rt_root, 30000) != -1);
  TEST(su_root_get_max_defer(rt->rt_root), 30000);

  if (su_timer_deferrable(t1, 1) == 0) {
    /*
     * If only a deferrable timer is set, su_root_step() should return
     * about the maximum defer time, which now defaults to 15 seconds
     */
    TEST(su_timer_set(t1, expire1, NULL), 0);
    defer = su_root_step(rt->rt_root, 0);
    TEST_1(defer > 100);
  }
  else {
    TEST(su_timer_set(t1, expire1, NULL), 0);
  }

  TEST(su_timer_set(t2, expire2, NULL), 0);

  while (su_root_step(rt->rt_root, 100) != SU_WAIT_FOREVER)
    ;

  TEST_1(rt->rt_t1 && rt->rt_t2);

  su_timer_destroy(t1);
  su_timer_destroy(t2);

  END();
}

static int set_execute_bit_and_return_3(void *void_rt)
{
  root_test_t *rt = void_rt;
  rt->rt_executed = 1;
  return 3;
}

static void deinit_simple_msg(su_msg_arg_t *arg)
{
  root_test_t *rt = *arg;
  rt->rt_msg_destroyed++;
}

static void receive_simple_msg(root_test_t *rt,
			       su_msg_r msg,
			       su_msg_arg_t *arg)
{
  assert(rt == *arg);
  rt->rt_msg_received =
    su_task_cmp(su_msg_from(msg), su_task_null) == 0 &&
    su_task_cmp(su_msg_to(msg), su_task_null) == 0;
}

static void expire1destroy(root_test_t *rt, su_timer_t *t, su_timer_arg_t *arg)
{
  (void)arg;
  rt->rt_t1 = 1;
  su_timer_destroy(t);
}

static int set_deferrable_timer(void *void_rt)
{
  root_test_t *rt = void_rt;
  su_timer_t *t1;

  TEST_1(t1 = su_timer_create(su_clone_task(rt->rt_clone), 100));
  TEST_1(su_timer_deferrable(t1, 1) == 0);
  TEST(su_timer_set(t1, expire1destroy, NULL), 0);

  return 0;
}

static int clone_test(root_test_t rt[1], int multithread)
{
  BEGIN();

  su_msg_r m = SU_MSG_R_INIT;
  int retval;

  su_root_threading(rt->rt_root, multithread);

  rt->rt_fail_init = 0;
  rt->rt_fail_deinit = 0;
  rt->rt_success_init = 0;
  rt->rt_success_deinit = 0;
  rt->rt_sent_reporter = 0;
  rt->rt_recv_reporter = 0;
  rt->rt_reported_reporter = 0;

  TEST(su_clone_start(rt->rt_root,
		      rt->rt_clone,
		      rt,
		      fail_init,
		      fail_deinit), SU_FAILURE);
  TEST_1(rt->rt_fail_init);
  TEST_1(rt->rt_fail_deinit);

  /* Defer longer than maximum allowed run time   */
  TEST_1(su_root_set_max_defer(rt->rt_root, ALARM_IN_SECONDS * 1000) != -1);
  TEST(su_root_get_max_defer(rt->rt_root), ALARM_IN_SECONDS * 1000);

  TEST(su_clone_start(rt->rt_root,
		      rt->rt_clone,
		      rt,
		      success_init,
		      success_deinit), SU_SUCCESS);
  TEST_1(rt->rt_success_init);
  TEST_1(!rt->rt_success_deinit);

  /* Test su_task_execute() */
  retval = -1;
  rt->rt_executed = 0;
  TEST(su_task_execute(su_clone_task(rt->rt_clone),
		       set_execute_bit_and_return_3, rt,
		       &retval), 0);
  TEST(retval, 3);
  TEST_1(rt->rt_executed);

  /* Deliver message with su_msg_send() */
  TEST(su_msg_new(m, sizeof &rt), 0);
  *su_msg_data(m) = rt;

  rt->rt_msg_received = 0;
  rt->rt_msg_destroyed = 0;

  TEST(su_msg_deinitializer(m, deinit_simple_msg), 0);
  TEST(su_msg_send_to(m, su_clone_task(rt->rt_clone), receive_simple_msg), 0);

  while (rt->rt_msg_destroyed == 0)
    su_root_step(rt->rt_root, 1);

  TEST(rt->rt_msg_received, 1);

  if (multithread) {
    TEST_1(su_task_is_running(su_clone_task(rt->rt_clone)));
  }
  else {
    TEST_1(!su_task_is_running(su_clone_task(rt->rt_clone)));
  }

  /* Test su_wakeup() */
  if (multithread) {
    retval = -1;
    rt->rt_t1 = 0;
    TEST(su_task_execute(su_clone_task(rt->rt_clone),
			 set_deferrable_timer, rt,
			 &retval), 0);
    TEST(retval, 0);

    while (rt->rt_t1 == 0) {
      TEST(su_root_step(rt->rt_root, 100), SU_WAIT_FOREVER);
      su_task_wakeup(su_clone_task(rt->rt_clone));
    }
  }

  /* Make sure 3-way handshake is done as expected */
  TEST(su_msg_create(m,
		     su_clone_task(rt->rt_clone),
		     su_root_task(rt->rt_root),
		     send_a_reporter_msg,
		     0), 0);
  TEST(su_msg_send(m), 0);

  TEST_VOID(su_clone_wait(rt->rt_root, rt->rt_clone));

  TEST_1(rt->rt_success_deinit);
  TEST_1(rt->rt_sent_reporter);
  TEST_1(rt->rt_recv_reporter);
  TEST_1(rt->rt_reported_reporter);

  rt->rt_recv_reporter = 0;

  /* Make sure we can handle messages done as expected */
  TEST(su_msg_create(m,
		     su_root_task(rt->rt_root),
		     su_task_null,
		     receive_a_reporter,
		     0), 0);
  TEST(su_msg_send(m), 0);
  su_root_step(rt->rt_root, 0);
  TEST_1(rt->rt_recv_reporter);

  rt->rt_success_init = 0;
  rt->rt_success_deinit = 0;

  END();
}

void usage(int exitcode)
{
  fprintf(stderr,
	  "usage: %s [-v] [-a]\n",
	  name);
  exit(exitcode);
}

#if HAVE_ALARM
#include <signal.h>

static RETSIGTYPE sig_alarm(int s)
{
  fprintf(stderr, "%s: FAIL! test timeout!\n", name);
  exit(1);
}
#endif

int main(int argc, char *argv[])
{
  root_test_t *rt, rt0[1] = {{{ SU_HOME_INIT(rt0) }}}, rt1[1];
  int retval = 0;
  int no_alarm = 0;
  int i;

  struct {
    su_port_create_f *create;
    su_clone_start_f *start;
    char const *name;
  } prefer[] =
      {
	{ NULL, NULL, "default" },
#if HAVE_EPOLL
	{ su_epoll_port_create, su_epoll_clone_start, "epoll", },
#endif
#if HAVE_KQUEUE
	{ su_kqueue_port_create, su_kqueue_clone_start, "kqueue", },
#endif
#if HAVE_SYS_DEVPOLL_H
	{ su_devpoll_port_create, su_devpoll_clone_start, "devpoll", },
#endif
#if HAVE_POLL_PORT
	{ su_poll_port_create, su_poll_clone_start, "poll" },
#endif
#if HAVE_SELECT
	{ su_select_port_create, su_select_clone_start, "select" },
#endif
	{ NULL, NULL }
      };

  rt = rt0;
  rt->rt_family = AF_INET;

  for (i = 1; argv[i]; i++) {
    if (strcmp(argv[i], "-v") == 0)
      rt->rt_flags |= tst_verbatim;
    else if (strcmp(argv[i], "-a") == 0)
      rt->rt_flags |= tst_abort;
    else if (strcmp(argv[i], "--no-alarm") == 0)
      no_alarm = 1;
#if SU_HAVE_IN6
    else if (strcmp(argv[i], "-6") == 0)
      rt->rt_family = AF_INET6;
#endif
    else
      usage(1);
  }

#if HAVE_ALARM
  if (!no_alarm) {
    signal(SIGALRM, sig_alarm);
    alarm(ALARM_IN_SECONDS);
  }
#endif

#if HAVE_OPEN_C
  rt->rt_flags |= tst_verbatim;
#endif

  i = 0;

  su_init();

  do {
    rt = rt1, *rt = *rt0;

    retval |= test_api(rt);

    retval |= init_test(rt, prefer[i].name, prefer[i].create, prefer[i].start);
    retval |= register_test(rt);
    retval |= event_test(rt);
    retval |= timer_test(rt);
    retval |= clone_test(rt, 1);
    retval |= clone_test(rt, 0);
    retval |= deinit_test(rt);
  } while (prefer[++i].create);

  su_deinit();

  return retval;
}
