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
 * @CFILE su_base_port.c
 *
 * OS-Independent Socket Syncronization Interface.
 *
 * This looks like nth reincarnation of "reactor".  It implements the
 * poll/select/WaitForMultipleObjects and message passing functionality.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#define su_base_port_s su_port_s
#define SU_CLONE_T su_msg_t

#include "sofia-sip/su.h"
#include "su_port.h"
#include "sofia-sip/su_alloc.h"

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#if 1
#define PORT_REFCOUNT_DEBUG(x) ((void)0)
#else
#define PORT_REFCOUNT_DEBUG(x)  printf x
#endif

static int su_base_port_execute_msgs(su_msg_t *queue);

/**@internal
 *
 * Initialize a message port.
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 */
int su_base_port_init(su_port_t *self, su_port_vtable_t const *vtable)
{
  if (self) {
    self->sup_vtable = vtable;
    self->sup_tail = &self->sup_head;
    self->sup_max_defer = 15 * 1000;

    return su_port_obtain(self);
  }

  return -1;
}

/** @internal Deinit a base implementation of port. */
void su_base_port_deinit(su_port_t *self)
{
  if (su_port_own_thread(self))
    su_port_release(self);
}

void su_base_port_lock(su_port_t *self, char const *who)
{
}

void su_base_port_unlock(su_port_t *self, char const *who)
{
}

/** @internal Dummy implementation of su_port_thread() method.
 *
 * Currently this is only used if SU_HAVE_PTHREADS is 0.
 */
int su_base_port_thread(su_port_t const *self,
			enum su_port_thread_op op)
{
  switch (op) {

  case su_port_thread_op_is_obtained:
    return 2;			/* Current thread has obtained the port */

  case su_port_thread_op_release:
    return errno = ENOSYS, -1;

  case su_port_thread_op_obtain:
    return 0;			/* Allow initial obtain */

  default:
    return errno = ENOSYS, -1;
  }
}

void su_base_port_incref(su_port_t *self, char const *who)
{
  su_home_ref(self->sup_home);
  PORT_REFCOUNT_DEBUG(("incref(%p) to %u by %s\n", self,
		       su_home_refcount(self->sup_home), who));
}

int su_base_port_decref(su_port_t *self, int blocking, char const *who)
{
  int zapped = su_home_unref(self->sup_home);

  PORT_REFCOUNT_DEBUG(("%s(%p) to %u%s by %s\n",
		       blocking ? "zapref" : "decref",
		       self, zapped ? 0 : su_home_refcount(self->sup_home),
		       blocking && !zapped ? " FAILED" :"",
		       who));

  /* We should block until all references are destroyed */
  if (blocking)
    /* ...but we just abort() */
    assert(zapped);

  return zapped;
}

struct _GSource *su_base_port_gsource(su_port_t *self)
{
  return NULL;
}

/** @internal Send a message to the port.
 *
 * @retval 0 if there are other messages in queue, too
 * @retval -1 upon an error
 */
int su_base_port_send(su_port_t *self, su_msg_r rmsg)
{
  if (self) {
    int wakeup;

    su_port_lock(self, "su_port_send");

    wakeup = self->sup_head == NULL;

    *self->sup_tail = rmsg[0]; rmsg[0] = NULL;
    self->sup_tail = &(*self->sup_tail)->sum_next;

    su_port_unlock(self, "su_port_send");

    if (wakeup > 0)
      su_port_wakeup(self);

    return 0;
  }
  else {
    su_msg_destroy(rmsg);
    return -1;
  }
}

/** @internal
 * Execute the messages in the incoming queue.
 *
 * @param self - pointer to a port object
 *
 * @retval Number of messages executed
 */
int su_base_port_getmsgs(su_port_t *self)
{
  if (self->sup_head) {
    su_msg_t *queue;

    su_port_lock(self, "su_base_port_getmsgs");

    queue = self->sup_head;
    self->sup_tail = &self->sup_head;
    self->sup_head = NULL;

    su_port_unlock(self, "su_base_port_getmsgs");

    return su_base_port_execute_msgs(queue);
  }

  return 0;
}


int su_base_port_getmsgs_from(su_port_t *self, su_port_t *from)
{
  su_msg_t *msg, *selected;
  su_msg_t **next = &self->sup_head, **tail= &selected;

  if (!*next)
    return 0;

  su_port_lock(self, "su_base_port_getmsgs_from_port");

  while (*next) {
    msg = *next;

    if (msg->sum_from->sut_port == from) {
      *tail = msg, *next = msg->sum_next, tail = &msg->sum_next;
    }
    else
      next = &msg->sum_next;
  }

  *tail = NULL, self->sup_tail = next;

  su_port_unlock(self, "su_base_port_getmsgs_from_port");

  return su_base_port_execute_msgs(selected);
}

static
int su_base_port_getmsgs_of_root(su_port_t *self, su_root_t *root)
{
  su_msg_t *msg, *selected;
  su_msg_t **next = &self->sup_head, **tail= &selected;

  if (!*next)
    return 0;

  su_port_lock(self, "su_base_port_getmsgs_of_root");

  while (*next) {
    msg = *next;

    if (msg->sum_from->sut_root == root ||
	msg->sum_to->sut_root == root) {
      *tail = msg, *next = msg->sum_next, tail = &msg->sum_next;
    }
    else
      next = &msg->sum_next;
  }

  *tail = NULL, self->sup_tail = next;

  su_port_unlock(self, "su_base_port_getmsgs_of_root");

  return su_base_port_execute_msgs(selected);
}

static int su_base_port_execute_msgs(su_msg_t *queue)
{
  su_msg_t *msg;
  int n = 0;

  for (msg = queue; msg; msg = queue) {
    su_msg_f f = msg->sum_func;

    queue = msg->sum_next, msg->sum_next = NULL;

    if (f) {
      su_root_t *root = msg->sum_to->sut_root;

      if (msg->sum_to->sut_port == NULL)
	msg->sum_to->sut_root = NULL;
      f(SU_ROOT_MAGIC(root), &msg, msg->sum_data);
    }

    su_msg_delivery_report(&msg);
    n++;
  }

  return n;
}

/** @internal Enable multishot mode.
 *
 * The function su_port_multishot() enables, disables or queries the
 * multishot mode for the port. The multishot mode determines how the events
 * are scheduled by port. If multishot mode is enabled, port serves all the
 * sockets that have received network events. If it is disabled, the
 * socket events are server one at a time.
 *
 * @param self      pointer to port object
 * @param multishot multishot mode (0 => disables, 1 => enables, -1 => query)
 *
 * @retval 0 multishot mode is disabled
 * @retval 1 multishot mode is enabled
 * @retval -1 an error occurred
 */
int su_base_port_multishot(su_port_t *self, int multishot)
{
  return 0;
}

/** @internal Main loop.
 *
 * The function @c su_port_run() waits for wait objects and the timers
 * associated with the port object.  When any wait object is signaled or
 * timer is expired, it invokes the callbacks, and returns waiting.
 *
 * The function @c su_port_run() runs until @c su_port_break() is called
 * from a callback.
 *
 * @param self     pointer to port object
 *
 */
void su_base_port_run(su_port_t *self)
{
  su_duration_t tout = 0, tout2 = 0;

  assert(su_port_own_thread(self));

  for (self->sup_running = 1; self->sup_running;) {
    tout = self->sup_max_defer;

    if (self->sup_prepoll)
      self->sup_prepoll(self->sup_pp_magic, self->sup_pp_root);

    if (self->sup_head)
      self->sup_vtable->su_port_getmsgs(self);

    if (self->sup_timers || self->sup_deferrable) {
      su_time_t now = su_now();
      su_timer_expire(&self->sup_timers, &tout, now);
      su_timer_expire(&self->sup_deferrable, &tout2, now);
    }

    if (!self->sup_running)
      break;

    if (self->sup_head)      /* if there are messages do a quick wait */
      tout = 0;

    self->sup_vtable->su_port_wait_events(self, tout);
  }
}

#if tuning
/* This version can help tuning... */
void su_base_port_run_tune(su_port_t *self)
{
  int i;
  int timers = 0, messages = 0, events = 0;
  su_duration_t tout = 0, tout2 = 0;
  su_time_t started = su_now(), woken = started, bedtime = woken;

  assert(su_port_own_thread(self));

  for (self->sup_running = 1; self->sup_running;) {
    tout = self->sup_max_defer;

    timers = 0, messages = 0;

    if (self->sup_prepoll)
      self->sup_prepoll(self->sup_pp_magic, self->sup_pp_root);

    if (self->sup_head)
      messages = self->sup_vtable->su_port_getmsgs(self);

    if (self->sup_timers || self->sup_deferrable) {
      su_time_t now = su_now();
      timers =
	su_timer_expire(&self->sup_timers, &tout, now) +
	su_timer_expire(&self->sup_deferrable, &tout2, now);
    }

    if (!self->sup_running)
      break;

    if (self->sup_head)      /* if there are messages do a quick wait */
      tout = 0;

    bedtime = su_now();

    events = self->sup_vtable->su_port_wait_events(self, tout);

    woken = su_now();

    if (messages || timers || events)
      SU_DEBUG_1(("su_port_run(%p): %.6f: %u messages %u timers %u "
		  "events slept %.6f/%.3f\n",
		  self, su_time_diff(woken, started), messages, timers, events,
		  su_time_diff(woken, bedtime), tout * 1e-3));

    if (!self->sup_running)
      break;
  }
}
#endif

/** @internal
 * The function @c su_port_break() is used to terminate execution of @c
 * su_port_run(). It can be called from a callback function.
 *
 * @param self     pointer to port
 *
 */
void su_base_port_break(su_port_t *self)
{
  self->sup_running = 0;
}

/** @internal
 * Check if port is running.
 *
 * @param self     pointer to port
 */
int su_base_port_is_running(su_port_t const *self)
{
  return self->sup_running != 0;
}

/** @internal Block until wait object is signaled or timeout.
 *
 * This function waits for wait objects and the timers associated with
 * the root object.  When any wait object is signaled or timer is
 * expired, it invokes the callbacks.
 *
 *   This function returns when a callback has been invoked or @c tout
 *   milliseconds is elapsed.
 *
 * @param self     pointer to port
 * @param tout     timeout in milliseconds
 *
 * @return
 *   Milliseconds to the next invocation of timer, or @c SU_WAIT_FOREVER if
 *   there are no active timers.
 */
su_duration_t su_base_port_step(su_port_t *self, su_duration_t tout)
{
  su_time_t now = su_now();

  assert(su_port_own_thread(self));

  if (self->sup_prepoll)
    self->sup_prepoll(self->sup_pp_magic, self->sup_pp_root);

  if (self->sup_head)
    self->sup_vtable->su_port_getmsgs(self);

  if (self->sup_timers)
    su_timer_expire(&self->sup_timers, &tout, now);

  /* XXX: why isn't the timeout ignored here? */
  if (self->sup_deferrable)
    su_timer_expire(&self->sup_deferrable, &tout, now);

  /* if there are messages do a quick wait */
  if (self->sup_head)
    tout = 0;

  if (self->sup_vtable->su_port_wait_events(self, tout))
    tout = 0;
  else
    tout = SU_WAIT_FOREVER;

  if (self->sup_head) {
    if (self->sup_vtable->su_port_getmsgs(self)) {
      /* Check for wait events that may have been generated by messages */
      if (self->sup_vtable->su_port_wait_events(self, 0))
	tout = 0;
    }
  }

  if (self->sup_timers || self->sup_deferrable) {
    su_duration_t tout2 = SU_WAIT_FOREVER;

    now = su_now();
    su_timer_expire(&self->sup_timers, &tout, now);
    su_timer_expire(&self->sup_deferrable, &tout2, now);

    if (tout == SU_WAIT_FOREVER && tout2 != SU_WAIT_FOREVER) {
      if (tout2 < self->sup_max_defer)
	tout2 = self->sup_max_defer;
      tout = tout2;
    }
  }

  if (self->sup_head)
    tout = 0;

  return tout;
}

/* =========================================================================
 * Pre-poll() callback
 */

int su_base_port_add_prepoll(su_port_t *self,
			     su_root_t *root,
			     su_prepoll_f *callback,
			     su_prepoll_magic_t *magic)
{
  if (self->sup_prepoll)
    return -1;

  self->sup_prepoll = callback;
  self->sup_pp_magic = magic;
  self->sup_pp_root = root;

  return 0;
}

int su_base_port_remove_prepoll(su_port_t *self,
				su_root_t *root)
{
  if (self->sup_pp_root != root)
    return -1;

  self->sup_prepoll = NULL;
  self->sup_pp_magic = NULL;
  self->sup_pp_root = NULL;

  return 0;
}

/* =========================================================================
 * Timers
 */

su_timer_queue_t *su_base_port_timers(su_port_t *self)
{
  return &self->sup_timers;
}

su_timer_queue_t *su_base_port_deferrable(su_port_t *self)
{
  return &self->sup_deferrable;
}

int su_base_port_max_defer(su_port_t *self,
			   su_duration_t *return_duration,
			   su_duration_t *set_duration)
{
  if (set_duration && *set_duration > 0)
    self->sup_max_defer = *set_duration;
  if (return_duration)
    *return_duration = self->sup_max_defer;
  return 0;
}

/* ======================================================================
 * Clones
 */

#define SU_TASK_COPY(d, s, by) (void)((d)[0]=(s)[0], \
  (s)->sut_port?(void)su_port_incref(s->sut_port, #by):(void)0)

static void su_base_port_clone_break(su_root_magic_t *m,
				     su_msg_r msg,
				     su_msg_arg_t *arg);

int su_base_port_start_shared(su_root_t *parent,
			      su_clone_r return_clone,
			      su_root_magic_t *magic,
			      su_root_init_f init,
			      su_root_deinit_f deinit)
{
  su_port_t *self = parent->sur_task->sut_port;
  su_root_t *child;

  child = su_salloc(su_port_home(self), sizeof *child);
  if (!child)
    return -1;

  child->sur_magic = magic;
  child->sur_deinit = deinit;
  child->sur_threading = parent->sur_threading;

  SU_TASK_COPY(child->sur_parent, su_root_task(parent),
	       su_base_port_clone_start);
  SU_TASK_COPY(child->sur_task, child->sur_parent,
	       su_base_port_clone_start);

  child->sur_task->sut_root = child;

  if (su_msg_create(return_clone,
		    child->sur_task, su_root_task(parent),
		    su_base_port_clone_break,
		    0) == 0 &&
      init(child, magic) == 0)
    return 0;

  su_msg_destroy(return_clone);
  su_root_destroy(child);
  return -1;
}

static void su_base_port_clone_break(su_root_magic_t *m,
				     su_msg_r msg,
				     su_msg_arg_t *arg)
{
  _su_task_t const *task = su_msg_to(msg);

  while (su_base_port_getmsgs_of_root(task->sut_port, task->sut_root))
    ;

  su_root_destroy(task->sut_root);
}

/**Wait for the clone to exit.
 * @internal
 *
 * Called by su_port_wait() and su_clone_wait()
 */
void su_base_port_wait(su_clone_r rclone)
{
  su_port_t *self;
  su_root_t *root_to_wait;

  assert(*rclone);

  self = su_msg_from(rclone)->sut_port;
  assert(self == su_msg_to(rclone)->sut_port);
  root_to_wait = su_msg_to(rclone)->sut_root;

  assert(rclone[0]->sum_func == su_base_port_clone_break);

  while (su_base_port_getmsgs_of_root(self, root_to_wait))
    ;
  su_root_destroy(root_to_wait);
  su_msg_destroy(rclone);
}

