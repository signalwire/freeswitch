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
 * @CFILE su_root.c
 * OS-independent synchronization interface.
 * @internal
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "sofia-sip/su.h"

#if SU_HAVE_PTHREADS
#include <pthread.h>
#endif

struct su_root_s;

#define SU_ROOT_MAGIC_T struct su_root_magic_s
#define SU_WAKEUP_ARG_T struct su_wakeup_arg_s
#define SU_TIMER_ARG_T  struct su_timer_arg_s

#include "su_port.h"
#include "sofia-sip/su_alloc.h"

/**@ingroup su_wait
 *
 * @page su_root_t Tasks and root objects
 *
 * A task is the basic execution unit for the Sofia event-driven programming
 * model. According to the model, the program can ask that the event loop
 * invokes a callback function when a certain event occurs. Such events
 * include @ref su_root_register "I/O activity", @ref su_timer_t "timers" or
 * a @ref su_msg_t "message" from other task. The event loop is run with
 * function su_root_run() or su_root_step().
 *
 * Root object gives access to the task control. The root object represents
 * the task to the code running within task. Through the root, the task code
 * can access its context object (magic) and thread-synchronization features
 * like wait objects, timers, and messages.
 *
 * When a message is sent between tasks, a task reference #su_task_r is used
 * to reprent the task address. Reference counting is used to make sure that
 * the task references stay valid.
 *
 * The public API contains following functions:
 *    - su_root_create() [Do not call from cloned task]
 *    - su_root_destroy() [Do not call from cloned task]
 *    - su_root_magic()
 *    - su_root_register()
 *    - su_root_deregister()
 *    - su_root_unregister()
 *    - su_root_threading()
 *    - su_root_run() [Do not call from cloned task]
 *    - su_root_break() [Do not call from cloned task]
 *    - su_root_step() [Do not call from cloned task]
 *    - su_root_get_max_defer()
 *    - su_root_set_max_defer()
 *    - su_root_task()
 *
 * New tasks can be created via su_clone_start() function.
 */

/**@ingroup su_wait
 *
 * @page su_root_register Registering Wait Objects
 *
 * When application expects I/O events, it can create a wait object and
 * register it, a callback function and a context pointer to the #su_root_t
 * object using the su_root_register() function. Whenever the wait object
 * receives an event, the registered @link ::su_wakeup_f callback function
 * @endlink is invoked.
 *
 * When successful, the su_root_register() returns an small non-negative
 * integer representing the registration. The registration can be
 * manipulated with su_root_eventmask() function, for instance, when sending
 * through a socket block, the application can add SU_WAIT_OUT event to the
 * mask.
 *
 * The registration can be removed using su_root_deregister() function.
 */

/**@ingroup su_wait
 *
 * Contains hint of number of sockets supported by su_root_t */
int su_root_size_hint = 64;

/* ====================================================================== */

_su_task_r su_task_new(su_task_r task, su_root_t *root, su_port_t *port);
int su_task_attach(su_task_r self, su_root_t *root);
int su_task_detach(su_task_r self);

int su_timer_reset_all(su_timer_t **t0, su_task_r);

/* =========================================================================
 * Tasks
 */

/** NULL task. */
su_task_r const su_task_null = SU_TASK_R_INIT;

#define SU_TASK_ZAP(t, f) \
  while (t->sut_port) { \
   su_port_decref(t->sut_port, #f); t->sut_port = NULL; break; }

#define SU_TASK_ZAPP(t, f) \
  do { if (t->sut_port) { \
   su_port_decref(t->sut_port, #f); t->sut_port = NULL; } \
   t->sut_root = NULL; } while(0)

/**
 * Initialize a task handle with su_task_null.
 *
 * @param task task handle
 *
 * @return A reference to the initialized task handle.
 */
_su_task_r su_task_init(su_task_r task)
{
  assert(task);

  memset(task, 0, sizeof(su_task_r));
  return task;
}

/**
 * Destroy a task handle
 *
 * @param task task handle
 */
void su_task_deinit(su_task_r task)
{
  assert(task);

  SU_TASK_ZAP(task, su_task_deinit);
  task->sut_root = NULL;
}

/**
 * Create a new task handle.
 *
 * @param task task reference
 * @param root pointer to root object
 * @param port pointer to port object
 *
 * @return New task handle.
 */
_su_task_r su_task_new(su_task_r task, su_root_t *root, su_port_t *port)
{
  assert(task);

  task->sut_root = root;
  if ((task->sut_port = port)) {
    su_port_incref(port, "su_task_new");
  }
  return task;
}

/**
 * Duplicates a task handle.
 *
 * @param  dst      destination task reference
 * @param  src      source task reference
 */
void su_task_copy(su_task_r dst, su_task_r const src)
{
  su_port_t *port;

  assert(src); assert(dst);

  SU_TASK_ZAP(dst, su_task_copy);

  port = src->sut_port;
  if (port) {
    su_port_incref(port, "su_task_copy");
  }

  dst[0] = src[0];
}

#define SU_TASK_COPY(d, s, by) (void)((d)[0]=(s)[0], \
  (s)->sut_port?(void)su_port_incref(s->sut_port, #by):(void)0)

/**
 * Moves a task handle.
 *
 * @param  dst      destination task reference
 * @param  src      source task reference
 */
void su_task_move(su_task_r dst, su_task_r src)
{
  SU_TASK_ZAP(dst, su_task_move);
  dst[0] = src[0];
  src->sut_port = 0;
  src->sut_root = 0;
}

/**
 * Compare two tasks with each other.
 *
 * @param a  First task
 * @param b  Second task
 *
 * @retval negative number, if a < b
 * @retval positive number, if a > b
 * @retval 0, if a == b.
 */
int su_task_cmp(su_task_r const a, su_task_r const b)
{
  intptr_t retval = (char *)a->sut_port - (char *)b->sut_port;

  if (retval == 0)
    retval = (char *)a->sut_root - (char *)b->sut_root;

  if (sizeof(retval) != sizeof(int)) {
    if (retval < 0)
      retval = -1;
    else if (retval > 0)
      retval = 1;
  }

  return (int)retval;
}

/**
 * Tests if a task is running.
 *
 * @param task  task handle
 *
 * @retval true (nonzero) if task is not stopped,
 * @retval zero if it is null or stopped.
 *
 * @note A task sharing thread with another task is considered stopped when
 * ever the the main task is stopped.
 */
int su_task_is_running(su_task_r const task)
{
  return task && task->sut_root && su_port_is_running(task->sut_port);
}

/** @internal
 * Attach a root object to the task handle.
 *
 * @param self task handle
 * @param root pointer to the root object
 *
 * @retval 0 if successful,
 * @retval -1 otherwise.
 */
int su_task_attach(su_task_r self, su_root_t *root)
{
  if (self->sut_port) {
    self->sut_root = root;
    return 0;
  }
  else
    return -1;
}

/**
 * Get root pointer attached to a task handle.
 *
 * @param self task handle
 *
 * @return
 * A pointer to root object attached to the task handle, or NULL if no root
 * object has been attached.
 */
su_root_t *su_task_root(su_task_r const self)
{
  if (self->sut_port) return self->sut_root; else return NULL;
}

#if 0
/** @internal
 * Detach a root pointer from task handle.
 * @bug Not used anymore.
 */
int su_task_detach(su_task_r self)
{
  self->sut_root = NULL;
  return 0;
}
#endif

/**
 * Return the timer list associated with given task.
 *
 * @param task task handle
 *
 * @return A timer list of the task.
 */
su_timer_queue_t *su_task_timers(su_task_r const task)
{
  return task->sut_port ? su_port_timers(task->sut_port) : NULL;
}

/**Return the queue for deferrable timers associated with given task.
 *
 * @param task task handle
 *
 * @return A timer list of the task.
 *
 * @NEW_1_12_11
 */
su_timer_queue_t *su_task_deferrable(su_task_r const task)
{
  return task ? su_port_deferrable(task->sut_port) : NULL;
}

/** Wakeup a task.
 *
 * Wake up a task. This function is mainly useful when using deferrable
 * timers executed upon wakeup.
 *
 * @param task task handle
 *
 * @retval 0 if succesful
 * @retval -1 upon an error
 *
 * @NEW_1_12_11
 */
int su_task_wakeup(su_task_r const task)
{
  return task ? su_port_wakeup(task->sut_port) : -1;
}

/** Execute the @a function by @a task thread.
 *
 * @retval 0 if successful
 * @retval -1 upon an error
 */
int su_task_execute(su_task_r const task,
		    int (*function)(void *), void *arg,
		    int *return_value)
{
  int dummy;

  if (function == NULL)
    return (errno = EFAULT), -1;

  if (return_value == NULL)
    return_value = &dummy;

  if (!su_port_own_thread(task->sut_port)) {
    return su_port_execute(task, function, arg, return_value);
  }
  else {
    int value = function(arg);

    if (return_value)
      *return_value = value;

    return 0;
  }
}

/* Note that is *not* necessary same as su_root_t,
 * as su_root_t can be extended */

#define sur_port sur_task->sut_port
#define sur_root sur_task->sut_root

#define SU_ROOT_OWN_THREAD(r) (r->sur_port && su_port_own_thread(r->sur_port))

/** Create a reactor object.
 *
 * Allocate and initialize the instance of su_root_t.
 *
 * @param magic     pointer to user data
 *
 * @return A pointer to allocated su_root_t instance, NULL on error.
 */
su_root_t *su_root_create(su_root_magic_t *magic)
{
  return su_root_create_with_port(magic, su_port_create());
}

/* Initializer used by su_root_clone() */
static int
su_root_clone_initializer(su_root_t *root,
			  su_root_magic_t *magic)
{
  *(su_root_t **)magic = root;
  return 0;
}

/** Create a a new root object sharing port/thread with existing one.
 *
 * Allocate and initialize the instance of su_root_t.
 *
 * @param magic     pointer to user data
 *
 * @return A pointer to allocated su_root_t instance, NULL on error.
 *
 * @NEW_1_12_11
 */
su_root_t *
su_root_clone(su_root_t *self, su_root_magic_t *magic)
{
  int threading = 0, error;
  su_clone_r clone;
  su_root_t *cloned = NULL;

  if (self == NULL)
    return NULL;

  threading = self->sur_threading, self->sur_threading = 0;
  error = su_clone_start(self, clone,
			 (void *)&cloned, su_root_clone_initializer, NULL);
  self->sur_threading = threading;

  if (error)
    return NULL;

  su_clone_forget(clone);	/* destroyed with su_root_destroy() */
  su_root_set_magic(cloned, magic);
  return cloned;
}

/**@internal
 *
 * Create a reactor object using given message port.
 *
 * Allocate and initialize the instance of su_root_t. Note that this
 * function always uses a reference to su_port_t, even when creating the
 * root fails.
 *
 * @param magic     pointer to user data
 * @param port      pointer to a message port
 *
 * @return A pointer to allocated su_root_t instance, NULL on error.
 */
su_root_t *su_root_create_with_port(su_root_magic_t *magic,
				    su_port_t *port)
{
  su_root_t *self;

  if (!port)
    return NULL;

  self = su_salloc(su_port_home(port), sizeof(struct su_root_s));

  if (self) {
    self->sur_magic = magic;
#if SU_HAVE_PTHREADS
    self->sur_threading = SU_HAVE_PTHREADS;
#else
    self->sur_threading = 0;
#endif
    /* This one creates a new reference to port */
    su_task_new(self->sur_task, self, port);
    /* ... so we zap the old one below */
  }

  su_port_decref(port, "su_root_create_with_port");

  return self;
}

/** Destroy a root object.
 *
 *  Stop and free an instance of su_root_t
 *
 * @param self     pointer to a root object.
 */
void su_root_destroy(su_root_t *self)
{
  su_port_t *port;
  int unregistered, reset;

  if (!self)
    return;

  assert(SU_ROOT_OWN_THREAD(self));

  self->sur_deiniting = 1;

  if (self->sur_deinit) {
    su_root_deinit_f deinit = self->sur_deinit;
    su_root_magic_t *magic = self->sur_magic;
    self->sur_deinit = NULL;
    deinit(self, magic);
  }

  port = self->sur_port; assert(port);

  unregistered = su_port_unregister_all(port, self);
  reset = su_timer_reset_all(su_task_timers(self->sur_task), self->sur_task);

  if (su_task_deferrable(self->sur_task))
    reset += su_timer_reset_all(su_task_deferrable(self->sur_task),
				self->sur_task);

  if (unregistered || reset)
    SU_DEBUG_1(("su_root_destroy: "
		"%u registered waits, %u timers\n",
		unregistered, reset));

  SU_TASK_ZAP(self->sur_parent, su_root_destroy);

  su_free(su_port_home(port), self);

  su_port_decref(port, "su_root_destroy");
}

/** Get instance name.
 *
 * @param self      pointer to a root object
 *
 * @return Instance name (e.g., "epoll", "devpoll", "select").
 *
 * @NEW_1_12_6.
 */
char const *su_root_name(su_root_t *self)
{
  if (!self)
    return (void)(errno = EFAULT), NULL;
  assert(self->sur_port);
  return su_port_name(self->sur_task->sut_port);
}

/** Set the context pointer.
 *
 *  Set the context pointer (magic) of a root object.
 *
 * @param self      pointer to a root object
 * @param magic     pointer to user data
 *
 * @retval 0  when successful,
 * @retval -1 upon error.
 */
int su_root_set_magic(su_root_t *self, su_root_magic_t *magic)
{
  if (self == NULL)
    return (void)(errno = EFAULT), -1;
  assert(SU_ROOT_OWN_THREAD(self));
  self->sur_magic = magic;
  return 0;
}

/** Set threading option.
 *
 *   Controls whether su_clone_start() creates a new thread.
 *
 * @param self      pointer to a root object
 * @param enable    if true, enable threading, if false, disable threading
 *
 * @return True if threading is enabled.
 */
int su_root_threading(su_root_t *self, int enable)
{
  if (self == NULL)
    return (void)(errno = EFAULT), -1;
  assert(SU_ROOT_OWN_THREAD(self));
#if SU_HAVE_PTHREADS
  self->sur_threading = enable = enable != 0;
  return enable;
#else
  return 0;
#endif
}

/** Get context pointer.
 *
 *  The function su_root_magic() returns the user context pointer that was
 *  given input to su_root_create() or su_root_set_magic().
 *
 * @param self      pointer to a root object
 *
 * @return A pointer to user data
 */
su_root_magic_t *su_root_magic(su_root_t *self)
{
  if (!self)
    return (void)(errno = EFAULT), NULL;

  return self->sur_magic;
}

/** Get a GSource */
struct _GSource *su_root_gsource(su_root_t *self)
{
  if (!self)
    return (void)(errno = EFAULT), NULL;
  assert(self->sur_port);

  return su_port_gsource(self->sur_port);
}

/** Register a su_wait_t object.
 *
 *  The function su_root_register() registers a su_wait_t object. The wait
 *  object, a callback function and a argument are stored to the root
 *  object. The callback function is called, when the wait object is
 *  signaled.
 *
 *  Please note if identical wait objects are inserted, only first one is
 *  ever signalled.
 *
 * @param self      pointer to root object
 * @param wait      pointer to wait object
 * @param callback  callback function pointer
 * @param arg       argument given to callback function when it is invoked
 * @param priority  relative priority of the wait object
 *                  (0 is normal, 1 important, 2 realtime)
 *
 * @return Nonzero index of the wait object, or -1 upon an error.
 */
int su_root_register(su_root_t *self,
		     su_wait_t *wait,
		     su_wakeup_f callback,
		     su_wakeup_arg_t *arg,
		     int priority)
{
  if (!self || !wait)
    return (void)(errno = EFAULT), -1;
  assert(self->sur_port);

  return su_port_register(self->sur_port, self, wait, callback, arg, priority);
}

/** Unregister a su_wait_t object.
 *
 *  The function su_root_unregister() unregisters a su_wait_t object. The
 *  wait object, a callback function and a argument are removed from the
 *  root object.
 *
 * @param self      pointer to root object
 * @param wait      pointer to wait object
 * @param callback  callback function pointer (may be NULL)
 * @param arg       argument given to callback function when it is invoked
 *                  (may be NULL)
 *
 * @return Nonzero index of the wait object, or -1 upon an error.
 */
int su_root_unregister(su_root_t *self,
		       su_wait_t *wait,
		       su_wakeup_f callback, /* XXX - ignored */
		       su_wakeup_arg_t *arg)
{
  if (!self || !wait)
    return (void)(errno = EFAULT), -1;
  assert(self->sur_port);

  return su_port_unregister(self->sur_port, self, wait, callback, arg);
}

/** Set maximum defer time.
 *
 * The deferrable timers can be deferred until the task is otherwise
 * activated, however, they are deferred no longer than the maximum defer
 * time. The maximum defer time determines also the maximum time during
 * which task waits for events while running.  The maximum defer time is 15
 * seconds by default.
 *
 * Cloned tasks inherit the maximum defer time.
 *
 * @param self pointer to root object
 * @param max_defer maximum defer time in milliseconds
 *
 * @retval 0 when successful
 * @retval -1 upon an error
 *
 * @sa su_timer_deferrable()
 *
 * @NEW_1_12_11
 */
int su_root_set_max_defer(su_root_t *self, su_duration_t max_defer)
{
  if (!self)
    return -1;

  return su_port_max_defer(self->sur_port, &max_defer, &max_defer);
}

/** Get maximum defer time.
 *
 * The deferrable timers can be deferred until the task is otherwise
 * activated, however, they are deferred no longer than the maximum defer
 * time. The maximum defer time is 15 seconds by default.
 *
 * @param root pointer to root object
 *
 * @return Maximum defer time
 *
 * @NEW_1_12_7
 */
su_duration_t su_root_get_max_defer(su_root_t const *self)
{
  su_duration_t max_defer = SU_WAIT_MAX;

  if (self != NULL)
    su_port_max_defer(self->sur_port, &max_defer, NULL);

  return max_defer;
}

/** Remove a su_wait_t registration.
 *
 *  The function su_root_deregister() deregisters a su_wait_t object. The
 *  wait object, a callback function and a argument are removed from the
 *  root object. The wait object is destroyed.
 *
 * @param self      pointer to root object
 * @param index     registration index
 *
 * @return Index of the wait object, or -1 upon an error.
 */
int su_root_deregister(su_root_t *self, int index)
{
  if (!self)
    return (void)(errno = EFAULT), -1;
  if (index == 0 || index == -1)
    return (void)(errno = EINVAL), -1;
  assert(self->sur_port);

  return su_port_deregister(self->sur_port, index);
}

/** Set mask for a registered event.
 *
 * The function su_root_eventmask() sets the mask describing events that can
 * signal the registered callback.
 *
 * @param self   pointer to root object
 * @param index  registration index
 * @param socket socket
 * @param events new event mask
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
int su_root_eventmask(su_root_t *self, int index, int socket, int events)
{
  if (!self)
    return (void)(errno = EFAULT), -1;
  if (index == 0 || index == -1)
    return (void)(errno = EINVAL), -1;
  assert(self->sur_port);

  return su_port_eventmask(self->sur_port, index, socket, events);
}

/** Set multishot mode.
 *
 * The function su_root_multishot() enables, disables or queries the
 * multishot mode for the root. The multishot mode determines how the events
 * are scheduled by root. If multishot mode is enabled, root serves all the
 * sockets that have received network events. If it is disables, only first
 * socket event is served.
 *
 * @param self      pointer to root object
 * @param multishot multishot mode (0 => disables, 1 => enables, -1 => query)
 *
 * @retval 0 multishot mode is disabled
 * @retval 1 multishot mode is enabled
 * @retval -1 an error occurred
 */
int su_root_multishot(su_root_t *self, int multishot)
{
  if (!self)
    return (void)(errno = EFAULT), -1;
  assert(self->sur_port);
  return su_port_multishot(self->sur_port, multishot);
}

/** Run event and message loop.
 *
 * The function su_root_run() runs the root main loop. The root loop waits
 * for wait objects and the timers associated with the root object. When any
 * wait object is signaled or timer is expired, it invokes the callbacks,
 * and returns waiting.
 *
 * This function returns when su_root_break() is called from a callback.
 *
 * @param self      pointer to root object
 *
 */
void su_root_run(su_root_t *self)
{
  if (!self)
    return /* (void)(errno = EFAULT), -1 */;
  assert(self->sur_port);

  /* return */ su_port_run(self->sur_port);
}

/** Terminate event loop.
 *
 *   The function su_root_break() is used to terminate execution of
 *   su_root_run(). It can be called from a callback function.
 *
 * @param self      pointer to root object
 */
void su_root_break(su_root_t *self)
{
  if (!self)
    return /* (void)(errno = EFAULT), -1 */;
  assert(self->sur_port);

  /* return */ su_port_break(self->sur_port);
}

/** Process events, timers and messages.
 *
 *   The function su_root_step() waits for wait objects and the timers
 *   associated with the root object.  When any wait object is signaled or
 *   timer is expired, it invokes the callbacks.
 *
 *   This function returns when a callback has been invoked or tout
 *   milliseconds is elapsed.
 *
 * @param self      pointer to root object
 * @param tout      timeout in milliseconds
 *
 * @return Milliseconds to the next invocation of timer
 * @retval SU_WAIT_FOREVER if there are no active timers or if there was an error
 */
su_duration_t su_root_step(su_root_t *self, su_duration_t tout)
{
  if (self == NULL)
    return (void)(errno = EFAULT), SU_WAIT_FOREVER;
  assert(self->sur_port);
  return su_port_step(self->sur_port, tout);
}

/**Run event and message loop for given duration.
 *
 * The function su_root_sleep() runs event loop for @a duration milliseconds.
 * The event loop waits for wait objects and the timers associated with the
 * @a root object.  When any wait object is signaled, timer is expired, or
 * message is received, it invokes the callbacks and returns waiting.
 *
 * @param self      pointer to root object
 * @param duration  milliseconds to run event loop
 *
 * @retval milliseconds until next timer expiration
 */
su_duration_t su_root_sleep(su_root_t *self, su_duration_t duration)
{
  su_duration_t retval, accrued = 0;
  su_time_t started;

  if (self == NULL)
    return (void)(errno = EFAULT), SU_WAIT_FOREVER;

  assert(self->sur_port);
  started = su_now();

  do {
    retval = su_port_step(self->sur_port, duration - accrued);
    accrued = su_duration(su_now(), started);
  } while (accrued < duration);

  return retval;
}

/** Check wait events in callbacks that take lots of time
 *
 * This function does a 0 timeout poll() and runs wait objects
 *
 * @param self pointer to root object
 */
int su_root_yield(su_root_t *self)
{
  if (self == NULL)
    return (void)(errno = EFAULT), SU_WAIT_FOREVER;
  assert(self->sur_port);

  return su_port_wait_events(self->sur_port, 0);
}

/** Get task reference.
 *
 * Retrieve the task reference related with the root object.
 *
 * @param self      a pointer to a root object
 *
 * @return A reference to the task object.
 */
_su_task_r su_root_task(su_root_t const *self)
{
  if (self)
    return self->sur_task;
  else
    return su_task_null;
}

/** Get parent task reference.
 *
 * Retrieve the task reference of the parent task associated with the root
 * object.
 *
 * @param self a pointer to a root object
 *
 * @return A reference to the parent task object.
 */
_su_task_r su_root_parent(su_root_t const *self)
{
  if (self)
    return self->sur_parent;
  else
    return su_task_null;
}

/** Add a pre-poll callback. */
int su_root_add_prepoll(su_root_t *root,
			su_prepoll_f *callback,
			su_prepoll_magic_t *magic)
{
  if (root == NULL)
    return (void)(errno = EFAULT), -1;
  assert(root->sur_port);

  return su_port_add_prepoll(root->sur_port, root, callback, magic);
}

/** Remove a pre-poll callback */
int su_root_remove_prepoll(su_root_t *root)
{
  if (root == NULL)
    return (void)(errno = EFAULT), -1;
  assert(root->sur_port);

  return su_port_remove_prepoll(root->sur_port, root);
}

/** Release the root port for other threads.
 *
 * @NEW_1_12_7
 */
int su_root_release(su_root_t *root)
{
  if (root == NULL || root->sur_port == NULL)
    return (void)(errno = EFAULT), -1;
  return su_port_release(root->sur_port);
}

/** Obtain the root port from other thread.
 *
 * @param root pointer to root object
 *
 * @retval 0 if successful
 * @retval -1 upon an error
 *
 * @ERRORS
 * @ERROR EFAULT
 * @NEW_1_12_7
 */
int su_root_obtain(su_root_t *root)
{
  if (root == NULL || root->sur_port == NULL)
    return (void)(errno = EFAULT), -1;
  return su_port_obtain(root->sur_port);
}

/**Check if a thread has obtained the root.
 *
 * @param root a pointer to root object
 *
 * @retval 2 if current thread has obtained the root
 * @retval 1 if an another thread  has obtained the root
 * @retval 0 if no thread has obtained the root
 * @retval -1 upon an error
 *
 * @NEW_1_12_7
 */
int su_root_has_thread(su_root_t *root)
{
  if (root == NULL || root->sur_port == NULL)
    return (void)(errno = EFAULT), -1;
  return su_port_has_thread(root->sur_port);
}

/* =========================================================================
 * Messages
 */

/**
 * Allocate a su message of given size.
 *
 * Allocate a su message with given data size.
 *
 * @param  rmsg   handle to the new message (may be uninitialized prior calling)
 * @param  size   size of the message data
 *
 * @retval  0 if successful,
 * @retval -1 if message allocation fails.
 *
 * @NEW_1_12_8
 */
int su_msg_new(su_msg_r rmsg, size_t size)
{
  su_msg_t *msg;
  size_t total = sizeof(*msg) + (size_t)size;

  *rmsg = msg = su_zalloc(NULL, (isize_t)total);
  if (!*rmsg)
    return -1;

  msg->sum_size = total;
  return 0;
}

/**
 * Allocates a message of given size.
 *
 * The function @c su_msg_create() allocates a message with given data size.
 * If successful, it moves the new message handle to the @c rmsg.
 *
 * @param  rmsg   handle to the new message (may be uninitialized prior calling)
 * @param  to     the recipient task
 * @param  from   the sender task
 * @param  wakeup function that is called when message is delivered
 * @param  size   size of the message data
 *
 * @retval  0 if successful,
 * @retval -1 if message allocation fails.
 */
int su_msg_create(su_msg_r        rmsg,
		  su_task_r const to,
		  su_task_r const from,
		  su_msg_f        wakeup,
		  isize_t         size)
{
  if (su_msg_new(rmsg, (size_t) size) == 0) {
    SU_TASK_COPY(rmsg[0]->sum_to, to, su_msg_create);
    SU_TASK_COPY(rmsg[0]->sum_from, from, su_msg_create);
    rmsg[0]->sum_func = wakeup;
    return 0;
  }

  return -1;
}

/** Add a delivery report function to a message.
 *
 * The delivery report funcgtion gets called by the sending task after the
 * message was delivered and the message function was executed. (The
 * su_root_t message delivery loop calls su_msg_delivery_report()
 *
 */
int su_msg_report(su_msg_r msg,
		  su_msg_f report)
{
  if (msg && msg[0] && msg[0]->sum_report == NULL) {
    msg[0]->sum_report = report;
    return 0;
  }

  return -1;
}

/** Add a deinitializer function to a message.
 *
 * The deinitializer function is called when the message gets destroyed. It
 * is called even if the message was never delivered. Note that the thread
 * destroying the message and calling the deinit function is not necessarily
 * the same that sent the message nor the original recipient.
 *
 * @param rmsg   message reference
 * @param deinit pointer to deinitializer function
 *
 * @NEW_1_12_8
 */
int su_msg_deinitializer(su_msg_r rmsg,
			 su_msg_deinit_function *deinit)
{
  if (rmsg && rmsg[0]) {
    rmsg[0]->sum_deinit = deinit;
    return 0;
  }
  return -1;
}

/**
 * Allocates a reply message of given size.
 *
 * @param reply     handle to the new message (may be uninitialized prior calling)
 * @param rmsg       the incoming message
 * @param wakeup    function that is called when message is delivered
 * @param size      size of the message data
 *
 * @retval 0 if successful,
 * @retval -1 otherwise.
 */

int su_msg_reply(su_msg_r reply, su_msg_cr rmsg,
		 su_msg_f wakeup, isize_t size)
{
  su_msg_r rmsg0;

  assert(rmsg != reply);

  *rmsg0 = *(su_msg_t **) rmsg;
  *reply = NULL;

  return su_msg_create(reply, su_msg_from(rmsg0), su_msg_to(rmsg0), wakeup, size);
}


/** Send a delivery report.
 *
 * If the sender has attached a delivery report function to message with
 * su_msg_report(), the message is returned to the message queue of the
 * sending task. The sending task calls the delivery report function when it
 * has received the message.
 */
void su_msg_delivery_report(su_msg_r rmsg)
{
  su_task_r swap;

  if (!rmsg || !rmsg[0])
    return;

  if (!rmsg[0]->sum_report) {
    su_msg_destroy(rmsg);
    return;
  }

  *swap = *rmsg[0]->sum_from;
  *rmsg[0]->sum_from = *rmsg[0]->sum_to;
  *rmsg[0]->sum_to = *swap;

  rmsg[0]->sum_func = rmsg[0]->sum_report;
  rmsg[0]->sum_report = NULL;
  su_msg_send(rmsg);
}

/** Save a message. */
void su_msg_save(su_msg_r save, su_msg_r rmsg)
{
  if (save) {
    if (rmsg)
      save[0] = rmsg[0];
    else
      save[0] = NULL;
  }
  if (rmsg)
    rmsg[0] = NULL;
}

/**
 * Destroys an unsent message.
 *
 * @param rmsg       message handle.
 */
void su_msg_destroy(su_msg_r rmsg)
{
  su_msg_t *msg;

  assert(rmsg);

  msg = rmsg[0], rmsg[0] = NULL;

  if (msg) {
    SU_TASK_ZAP(msg->sum_to, su_msg_destroy);
    SU_TASK_ZAP(msg->sum_from, su_msg_destroy);

    if (msg->sum_deinit)
      msg->sum_deinit(msg->sum_data);

    su_free(NULL, msg);
  }
}

/** Gets a pointer to the message data area.
 *
 * The function @c su_msg_data() returns a pointer to the message data
 * area. If @c rmsg contains a @c NULL handle, or message size is 0, @c NULL
 * pointer is returned.
 *
 * @param rmsg       message handle
 *
 * @return A pointer to the message data area is returned.
 */
su_msg_arg_t *su_msg_data(su_msg_cr rmsg)
{
  if (rmsg[0] && rmsg[0]->sum_size > sizeof(su_msg_t))
    return rmsg[0]->sum_data;
  else
    return NULL;
}

/** Get size of message data area. */
isize_t su_msg_size(su_msg_cr rmsg)
{
  return rmsg[0] ? rmsg[0]->sum_size - sizeof(su_msg_t) : 0;
}

/** Get sending task.
 *
 * Returns the task handle belonging to the sender of the message.
 *
 * If the message handle contains NULL the function @c su_msg_from
 * returns NULL.
 *
 * @param rmsg       message handle
 *
 * @return The task handle of the sender is returned.
 */
_su_task_r su_msg_from(su_msg_cr rmsg)
{
  return rmsg[0] ? rmsg[0]->sum_from : NULL;
}

/** Get destination task.
 *
 * The function @c su_msg_from returns the task handle belonging to the
 * recipient of the message.
 *
 * If the message handle contains NULL the function @c su_msg_to
 * returns NULL.
 *
 * @param rmsg       message handle
 *
 * @return The task handle of the recipient is returned.
 */
_su_task_r su_msg_to(su_msg_cr rmsg)
{
  return rmsg[0] ? rmsg[0]->sum_to : NULL;
}

/** Remove references to 'from' and 'to' tasks from a message.
 *
 * @param rmsg       message handle
 */
void su_msg_remove_refs(su_msg_cr rmsg)
{
  if (rmsg[0]) {
    su_task_deinit(rmsg[0]->sum_to);
    su_task_deinit(rmsg[0]->sum_from);
  }
}

/**Send a message.
 *
 * The function @c su_msg_send() sends the message. The message is added to
 * the recipients message queue, and recipient is waken up. The caller may
 * not alter the message or the data associated with it after the message
 * has been sent.
 *
 * @param rmsg message handle
 *
 * @retval 0 if signal was sent successfully or handle was @c NULL,
 * @retval -1 otherwise.
 */
int su_msg_send(su_msg_r rmsg)
{
  assert(rmsg);

  if (rmsg[0]) {
    su_msg_t *msg = rmsg[0];

    if (msg->sum_to->sut_port)
      return su_port_send(msg->sum_to->sut_port, rmsg);

    su_msg_destroy(rmsg);
    errno = EINVAL;
    return -1;
  }

  return 0;
}

/** Send message to the @a to_task and mark @a from_task as sender.
 *
 * @NEW_1_12_8
 */
SOFIAPUBFUN int su_msg_send_to(su_msg_r rmsg,
			       su_task_r const to_task,
			       su_msg_f wakeup)
{
  assert(rmsg); assert(to_task);

  if (rmsg[0]) {
    su_msg_t *msg = rmsg[0];

    if (wakeup)
      msg->sum_func = wakeup;

    if (msg->sum_to->sut_port &&
	msg->sum_to->sut_port != to_task->sut_port) {
      SU_TASK_ZAP(msg->sum_to, "su_msg_send_to");
    }

    if (to_task->sut_port != NULL) {
      msg->sum_to->sut_port = NULL;
      msg->sum_to->sut_root = to_task->sut_root;

      return su_port_send(to_task->sut_port, rmsg);
    }

    su_msg_destroy(rmsg);
    errno = EINVAL;
    return -1;
  }

  return 0;
}
