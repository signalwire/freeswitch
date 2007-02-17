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

typedef struct su_cloned_s {
  struct su_root_s *sc_root;
  int *sc_wait;
#if SU_HAVE_PTHREADS
  pthread_t  sc_tid;
  pthread_mutex_t sc_pause[1];
  pthread_cond_t sc_resume[1];
  int sc_paused;
#endif  
} su_cloned_t;

#define SU_ROOT_MAGIC_T struct su_root_magic_s
#define SU_WAKEUP_ARG_T struct su_wakeup_arg_s
#define SU_TIMER_ARG_T  struct su_timer_arg_s
#define SU_CLONE_T      su_msg_t
#define SU_MSG_ARG_T    struct su_cloned_s

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

/* =========================================================================
 * Tasks
 */

su_task_r const su_task_null = SU_TASK_R_INIT;

#define SU_TASK_ZAP(t, f) \
  while (t->sut_port) { \
   SU_PORT_DECREF(t->sut_port, f); t->sut_port = NULL; break; }

#define SU_TASK_ZAPP(t, f) \
  do { if (t->sut_port) { \
   SU_PORT_DECREF(t->sut_port, f); t->sut_port = NULL; } \
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

  memset(task, 0, sizeof(task));
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
    SU_PORT_INCREF(port, su_task_new);
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
    SU_PORT_INCREF(port, su_task_copy);
  }

  dst[0] = src[0];
}

#define SU_TASK_COPY(d, s, by) (void)((d)[0]=(s)[0], \
  (s)->sut_port?(void)SU_PORT_INCREF(s->sut_port, by):(void)0)

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
  intptr_t retval = a->sut_port - b->sut_port;
  retval = retval ? retval : (char *)a->sut_root - (char *)b->sut_root;

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
 */
int su_task_is_running(su_task_r const task)
{
  return 
    task && 
    task->sut_port && 
    task->sut_root;
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
 * @return A timer list of the task. If there are no timers, it returns
 * NULL.
 */
su_timer_t **su_task_timers(su_task_r const task)
{
  return task ? su_port_timers(task->sut_port) : NULL;
}

#if SU_HAVE_PTHREADS

struct su_task_execute
{
  pthread_mutex_t mutex[1];
  pthread_cond_t cond[1];
  int (*function)(void *);
  void *arg;
  int value;
};

static void _su_task_execute(su_root_magic_t *m,
			     su_msg_r msg,
			     su_msg_arg_t *a)
{
  struct su_task_execute *frame = *(struct su_task_execute **)a;
  pthread_mutex_lock(frame->mutex);
  frame->value = frame->function(frame->arg);
  frame->function = NULL;	/* Mark as completed */
  pthread_cond_signal(frame->cond);
  pthread_mutex_unlock(frame->mutex);
}

#endif

/** Execute by task thread
 *
 * @retval 0 if successful
 * @retval -1 upon an error
 */
int su_task_execute(su_task_r const task,
		    int (*function)(void *), void *arg,
		    int *return_value)
{
  if (function == NULL)
    return (errno = EFAULT), -1;

  if (!su_port_own_thread(task->sut_port)) {
#if SU_HAVE_PTHREADS
    int success;
    su_msg_r m = SU_MSG_R_INIT;
    struct su_task_execute frame = {
      { PTHREAD_MUTEX_INITIALIZER },
      { PTHREAD_COND_INITIALIZER },
      function, arg, 0
    };

    if (su_msg_create(m, task, su_task_null,
		      _su_task_execute, (sizeof &frame)) < 0)
      return -1;

    *(struct su_task_execute **)su_msg_data(m) = &frame;

    pthread_mutex_lock(frame.mutex);

    success = su_msg_send(m);

    if (success == 0)
      while (frame.function)
	pthread_cond_wait(frame.cond, frame.mutex);
    else
      su_msg_destroy(m);

    pthread_mutex_unlock(frame.mutex);
    pthread_mutex_destroy(frame.mutex);
    pthread_cond_destroy(frame.cond);

    if (return_value)
      *return_value = frame.value;

    return success;
#else
    return (errno = ENOSYS), -1;
#endif
  }
  else {
    int value = function(arg);

    if (return_value)
      *return_value = value;

    return 0;
  }
}

_su_task_r su_task_new(su_task_r task, su_root_t *root, su_port_t *port);
int su_task_attach(su_task_r self, su_root_t *root);
int su_task_detach(su_task_r self);

int su_timer_reset_all(su_timer_t **t0, su_task_r);

/**@ingroup su_wait
 * 
 * @page su_clone_t Clone Objects
 *
 * The process may be divided into many tasks via cloning. Several tasks may
 * run in context of one thread, or each task may be run by its own thread. 
 * However, only a single thread can execute code within a task. There can
 * be a 1-to-N mapping from thread to tasks. Thus, software using tasks can
 * be executed by multiple threads in a multithreaded environment and by a
 * single thread in a singlethreaded environment.
 * 
 * The clones are useful for handling tasks that can be executed by a
 * separate threads, but which do not block excessively. When threads are
 * not available or they are not needed, clones can also be run in a
 * single-threaded mode. Running in single-threaded mode is especially
 * useful while debugging.
 * 
 * A clone task is created with function su_clone_start(). Each clone has
 * its own root object (su_root_t), which holds a context pointer
 * (su_root_magic_t *). The context object can be different from that of 
 * parent task.
 *
 * When a clone is started, the clone initialization function is called. The
 * initialization function should do whatever initialization there is to be
 * performed, register I/O events and timers, and then return. If the
 * initialization is successful, the clone task reverts to run the event
 * loop and invoking the event callbacks until its parent stops it by
 * calling su_clone_wait() which invokes the deinit function. The clone task
 * is destroyed when the deinit function returns. 
 *
 * The public API consists of following functions:
 *    - su_clone_start()
 *    - su_clone_task()
 *    - su_clone_wait()
 *    - su_clone_forget()
 *
 * @note 
 * There is only one event loop for each thread which can be shared by
 * multiple clone tasks. Therefore, the clone tasks can not explicitly run
 * or step the event loop, but they are limited to event callbacks. A clone
 * task may not call su_root_break(), su_root_run() or su_root_step().
 */

static void su_root_deinit(su_root_t *self);

/* Note that is *not* necessary same as su_root_t,
 * as su_root_t can be extended */

#define sur_port sur_task->sut_port
#define sur_root sur_task->sut_root

#define SU_ROOT_OWN_THREAD(r) (su_port_own_thread(r->sur_port))

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

/** Create a reactor object using given message port.
 *
 * Allocate and initialize the instance of su_root_t.
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

  self = su_salloc(NULL, sizeof(struct su_root_s));

  if (self) {
    self->sur_magic = magic;
#if SU_HAVE_PTHREADS
    self->sur_threading = SU_HAVE_PTHREADS;
#else
    self->sur_threading = 0;
#endif
    su_task_new(self->sur_task, self, port);
  } else {
    su_port_decref(port, "su_root_create");
  }

  return self;
}

/** Destroy a synchronization object.
 * 
 *  Stop and free an instance of su_root_t
 *
 * @param self     pointer to a root object.
 */
void su_root_destroy(su_root_t *self)
{
  if (self) {
    assert(SU_ROOT_OWN_THREAD(self));
    su_root_deinit(self);
    su_free(NULL, self);
  }
}

/** @internal Deinitialize a synchronization object.
 *
 *  Deinitialize an instance of su_root_t
 *
 * @param self     pointer to a root object.
 */
static void su_root_deinit(su_root_t *self)
{
  self->sur_deiniting = 1;

  if (self->sur_deinit) {
    su_root_deinit_f deinit = self->sur_deinit;
    su_root_magic_t *magic = self->sur_magic;
    self->sur_deinit = NULL;
    deinit(self, magic);
  }

  if (self->sur_port) {
    int n_w = su_port_unregister_all(self->sur_port, self);
    int n_t = su_timer_reset_all(su_task_timers(self->sur_task), self->sur_task);

    if (n_w || n_t)
      SU_DEBUG_1(("su_root_deinit: "
		  "%u registered waits, %u timers\n", n_w, n_t));
  }

  SU_TASK_ZAP(self->sur_parent, su_root_deinit);
  SU_TASK_ZAP(self->sur_task, su_root_deinit);
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
  assert(SU_ROOT_OWN_THREAD(self));

  if (self) {
    self->sur_magic = magic;
  }

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
  if (self) {
    assert(SU_ROOT_OWN_THREAD(self));

#if SU_HAVE_PTHREADS
    self->sur_threading = enable = enable != 0;
    return enable;
#endif
  }

  return 0;
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
  return self ? self->sur_magic : NULL;
}

/** Get a GSource */
struct _GSource *su_root_gsource(su_root_t *self)
{
  return self ? su_port_gsource(self->sur_port) : NULL;
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
  assert(self && self->sur_port);

  if (!self || !self->sur_port)
    return -1;

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
  assert(self && self->sur_port);

  if (!self || !self->sur_port)
    return -1;

  return su_port_unregister(self->sur_port, self, wait, callback, arg);
}

/** Remove a su_wait_t registration.
 *
 *  The function su_root_deregister() deregisters a su_wait_t object. The
 *  wait object, a callback function and a argument are removed from the
 *  root object. The wait object is destroyed.
 *
 * @param self      pointer to root object
 * @param i         registration index
 *
 * @return Index of the wait object, or -1 upon an error.
 */
int su_root_deregister(su_root_t *self, int i)
{
  if (i == 0 || i == -1)
    return -1;

  assert(self && self->sur_port);

  if (!self || !self->sur_port)
    return -1;

  return su_port_deregister(self->sur_port, i);
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
  assert(self && self->sur_port);

  if (!self || !self->sur_port)
    return -1;

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
  if (self && self->sur_port) {
    return su_port_multishot(self->sur_port, multishot);
  } else {
    return (errno = EINVAL), -1;
  }
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
  assert(self && self->sur_port);

  if (self && self->sur_port)
    su_port_run(self->sur_port);
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
  assert(self && self->sur_port);

  if (self && self->sur_port)
    su_port_break(self->sur_port);
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
 * @return Milliseconds to the next invocation of timer, or SU_WAIT_FOREVER
 *         if there are no active timers.
 */
su_duration_t su_root_step(su_root_t *self, su_duration_t tout)
{
  assert(self && self->sur_port);

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
 */
su_duration_t su_root_sleep(su_root_t *self, su_duration_t duration)
{
  su_duration_t retval, accrued = 0;
  su_time_t started = su_now();

  assert(self && self->sur_port);

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
  if (self && self->sur_task[0].sut_port) {
    su_port_t *port = self->sur_task[0].sut_port;
    /* Make sure we have su_port_yield extension */
    if (port->sup_vtable->su_vtable_size >= 
	offsetof(su_port_vtable_t, su_port_yield) 
	&& port->sup_vtable->su_port_yield)
      return port->sup_vtable->su_port_yield(port);
  }
  errno = EINVAL;
  return -1;
}

/** Get task reference.
 *
 *   The function su_root_task() is used to retrieve the task reference
 *   (PId) related with the root object.
 *
 * @param self      a pointer to a root object
 *
 * @return The function su_root_task() returns a reference to the task
 *         object.
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
 *   The function su_root_parent() is used to retrieve the task reference
 *   (PId) of the parent task.
 *
 * @param self      a pointer to a root object
 *
 * @return The function su_root_parent() returns a reference to the parent
 *         task object.
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
  if (root == NULL || root->sur_port == NULL)
    return -1;

  return su_port_add_prepoll(root->sur_port, root, callback, magic);
}

/** Remove a pre-poll callback */
int su_root_remove_prepoll(su_root_t *root)
{
  if (root == NULL || root->sur_port == NULL)
    return -1;

  return su_port_remove_prepoll(root->sur_port, root);
}

/* ========================================================================
 * su_clone_t
 */

/* - su_clone_forget() */

#if SU_HAVE_PTHREADS
struct clone_args
{
  su_root_t      * self;
  su_root_init_f   init;
  su_root_deinit_f deinit;
  pthread_mutex_t  mutex;
  pthread_cond_t   cv;
  int              retval;
  su_msg_r         clone;
  su_root_t const *parent;
};

static void su_clone_report2(su_root_magic_t *m,
			     su_msg_r msg,
			     su_cloned_t *sc);

static void su_clone_signal_parent(void *varg)
{
  struct clone_args *arg = (struct clone_args *)varg;

  pthread_mutex_lock(&arg->mutex);
  pthread_cond_signal(&arg->cv);
  pthread_mutex_unlock(&arg->mutex);
}

/** Message function for clone message.
 *
 * This calls the clone task deinitialization function, which should make
 * sure that no more messages are sent by clone task.
 *
 * @sa su_clone_wait()
 */
static void su_clone_break(su_root_magic_t *m,
			   su_msg_r msg,
			   su_cloned_t *sc)
{
  su_root_t *root = sc->sc_root;

  root->sur_deiniting = 1;

  if (root->sur_deinit) {
    su_root_deinit_f deinit = root->sur_deinit;
    su_root_magic_t *magic = root->sur_magic;
    root->sur_deinit = NULL;
    deinit(root, magic);
  }
}

/** Delivery report function for clone message.
 *
 * This is executed by parent task. This is the last message sent by clone task.
 */
static void su_clone_report(su_root_magic_t *m,
			    su_msg_r msg,
			    su_cloned_t *sc)
{
  su_msg_report(msg, su_clone_report2);
}

/** Back delivery report function for clone message.
 *
 * This is executed by clone task. It completes the three way handshake and
 * it is used to signal clone that it can destroy its port.
 */
static void su_clone_report2(su_root_magic_t *m,
			    su_msg_r msg,
			    su_cloned_t *sc)
{
  su_root_break(sc->sc_root);
  if (sc->sc_wait)
    *sc->sc_wait = 0;
}

static void *su_clone_main(void *varg)
{
  struct clone_args *arg = (struct clone_args *)varg;
  su_root_t *self = arg->self;
  su_port_t *port;
  su_cloned_t *sc;

  pthread_cleanup_push(su_clone_signal_parent, varg);

#if SU_HAVE_WINSOCK
  su_init();
#endif

  port = su_port_create();
  if (!port)
    pthread_exit(NULL);
  su_port_threadsafe(port);
  SU_PORT_INCREF(port, su_clone_main);

  /* Change task ownership */
  SU_PORT_INCREF(self->sur_task->sut_port = port, su_clone_main);
  self->sur_task->sut_root = self;

  if (su_msg_create(arg->clone,
		    self->sur_task, su_root_task(arg->parent),
		    su_clone_break, sizeof(self)) != 0) {
    su_port_decref(self->sur_port, "su_clone_main");
    self->sur_port = NULL;
    pthread_exit(NULL);
  }

  su_msg_report(arg->clone, su_clone_report);

  sc = su_msg_data(arg->clone);
  sc->sc_root = self;
  sc->sc_tid = pthread_self();

  pthread_mutex_init(sc->sc_pause, NULL);
  pthread_cond_init(sc->sc_resume, NULL);
  pthread_mutex_lock(sc->sc_pause);

  if (arg->init && arg->init(self, self->sur_magic) != 0) {
    if (arg->deinit)
      arg->deinit(self, self->sur_magic);
    su_msg_destroy(arg->clone);
    su_port_decref(self->sur_port, "su_clone_main");
    self->sur_port = NULL;
    pthread_exit(NULL);
  }

  arg->retval = 0;

  pthread_cleanup_pop(1);  /* signal change of ownership */

  su_root_run(self);   /* Do the work */

  su_root_destroy(self);   /* Cleanup root */   

  SU_PORT_ZAPREF(port, su_clone_main);

#if SU_HAVE_WINSOCK
  su_deinit();
#endif

  return NULL;
}
#endif

static void su_clone_xyzzy(su_root_magic_t *m,
			   su_msg_r msg,
			   su_cloned_t *sc)
{
  su_root_destroy(sc->sc_root);
  if (sc->sc_wait)
    *sc->sc_wait = 0;
}

/** Start a clone task.
 *
 * The function su_clone_start() allocates and initializes a sub-task. 
 * Depending on the settings, a separate thread may be created to execute
 * the sub-task. The sub-task is represented by clone handle to the rest of
 * the application. The function su_clone_start() returns the clone handle
 * in @a return_clone. The clone handle is used to communicate with the
 * newly created clone task using messages.
 *
 * A new #su_root_t object is created for the sub-task with the @a magic as
 * the root context pointer. Because the sub-task may or may not have its
 * own thread, all its activity must be scheduled via this root object. In
 * other words, the sub-task can be schedule
 * -# I/O events with su_root_register()
 * -# timers with su_timer_set(), su_timer_set_at() or su_timer_run()
 * -# messages with su_msg_send().
 *
 * Messages can also be used to pass information between tasks or threads.
 *
 * In multi-threaded implementation, su_clone_start() launches a new thread,
 * and the initialization routine is executed by this newly created thread. 
 * The calling thread blocks until the initialization routine completes. If
 * the initialization routine returns #su_success (0), the sub-task is
 * considered to be created successfully. After the successful
 * initialization, the sub-task continues to execeute the function
 * su_root_run().
 *
 * In single-threaded implementations, just a new root object is created. 
 * The initialization routine is called directly from su_clone_start().
 *
 * If the initalization function @a init fails, the sub-task (either the
 * newly created thread or the current thread executing the su_clone_start()
 * function) calls the deinitialization function, and su_clone_start()
 * returns NULL.
 *
 * @param parent   root to be cloned (may be NULL if multi-threaded)
 * @param return_clone reference to a clone [OUT]
 * @param magic    pointer to user data
 * @param init     initialization function
 * @param deinit   deinitialization function
 *
 * @return 0 if successfull, -1 upon an error.
 *
 * @sa su_root_threading(), su_clone_task(), su_clone_stop(), su_clone_wait(),
 * su_clone_forget().
 */
int su_clone_start(su_root_t *parent,
		   su_clone_r return_clone,
		   su_root_magic_t *magic,
		   su_root_init_f init,
		   su_root_deinit_f deinit)
{
  su_root_t *child;
  int retval = -1;

  if (parent) {
    assert(SU_ROOT_OWN_THREAD(parent));
    assert(parent->sur_port);
  }
#if !SU_HAVE_PTHREADS
  else {
    /* if we don't have threads, we *must* have parent root */
    return -1;
  }
#endif

  child = su_salloc(NULL, sizeof(struct su_root_s));

#if SU_HAVE_PTHREADS
  if (child && (parent == NULL || parent->sur_threading)) {
    struct clone_args arg = {
      NULL, NULL, NULL,
      PTHREAD_MUTEX_INITIALIZER,
      PTHREAD_COND_INITIALIZER,
      -1,
      SU_MSG_R_INIT,
      NULL
    };

    int thread_created = 0;
    pthread_t tid;

    su_port_threadsafe(parent->sur_port);

    arg.self = child;
    arg.init = init;
    arg.deinit = deinit;
    arg.parent = parent;

    child->sur_magic = magic;
    child->sur_deinit = deinit;
    child->sur_threading = parent->sur_threading;

    SU_TASK_COPY(child->sur_parent, su_root_task(parent), su_clone_start);

    pthread_mutex_lock(&arg.mutex);
    if (pthread_create(&tid, NULL, su_clone_main, &arg) == 0) {
      pthread_cond_wait(&arg.cv, &arg.mutex);
      thread_created = 1;
    }
    pthread_mutex_unlock(&arg.mutex);

    if (arg.retval != 0) {
      if (thread_created)
	pthread_join(tid, NULL);
      su_root_destroy(child), child = NULL;
    }
    else {
      retval = 0;
      *return_clone = *arg.clone;
    }
  } else
#endif
  if (child) {
    assert(parent);

    child->sur_magic = magic;
    child->sur_deinit = deinit;
    child->sur_threading = parent->sur_threading;

    SU_TASK_COPY(child->sur_parent, su_root_task(parent), su_clone_start);
    SU_TASK_COPY(child->sur_task, child->sur_parent, su_clone_start);
    su_task_attach(child->sur_task, child);

    if (su_msg_create(return_clone,
		      child->sur_task, su_root_task(parent),
		      su_clone_xyzzy, sizeof(child)) == 0) {
      if (init == NULL || init(child, magic) == 0) {
	su_cloned_t *sc = su_msg_data(return_clone);
	sc->sc_root = child;
#if SU_HAVE_PTHREADS
	sc->sc_tid = pthread_self();
	pthread_mutex_init(sc->sc_pause, NULL);
	pthread_cond_init(sc->sc_resume, NULL);
	pthread_mutex_lock(sc->sc_pause);
#endif
	retval = 0;
      } else {
	if (deinit)
	  deinit(child, magic);
	su_msg_destroy(return_clone);
	su_root_destroy(child), child = NULL;
      }
    }
    else {
      su_root_destroy(child), child = NULL;
    }
  }

  return retval;
}

/** Get reference to clone task.
 * 
 * @param clone Clone pointer
 *
 * @return A reference to the task structure of the clone.
 */
_su_task_r su_clone_task(su_clone_r clone)
{
  return su_msg_to(clone);
}

/**Forget the clone.
 * 
 * Normally, the clone task executes until it is stopped.  If the parent
 * task does not need to stop the task, it can "forget" the clone.  The
 * clone exits independently of the parent task.
 *
 * @param rclone Reference to the clone.
 */
void su_clone_forget(su_clone_r rclone)
{
  su_msg_destroy(rclone);
}

/** Stop the clone.
 *
 * @deprecated. Use su_clone_wait().
 */
void su_clone_stop(su_clone_r rclone)
{
  su_msg_send(rclone);
}

/** Stop a clone and wait until it is has completed.
 *
 * The function su_clone_wait() is used to stop the clone task and wait
 * until it has cleaned up. The clone task is destroyed asynchronously. The
 * parent sends a message to clone, clone deinitializes itself and then
 * replies. After the reply message is received by the parent, it will send
 * a third message back to clone.
 *
 * The parent destroy all messages to or from clone task before calling
 * su_clone_wait(). The parent task may not send any messages to the clone
 * after calling su_clone_wait(). The su_clone_wait() function blocks until
 * the cloned task is destroyed. During that time, the parent task must be
 * prepared to process all the messages sent by clone task. This includes
 * all the messages sent by clone before destroy message reached the clone.
 */
void su_clone_wait(su_root_t *root, su_clone_r rclone)
{
  su_cloned_t *sc = su_msg_data(rclone);

  if (sc) {
#if SU_HAVE_PTHREADS
    pthread_t clone_tid = sc->sc_tid;
#endif
    int one = 1;
    /* This does 3-way handshake. 
     * First, su_clone_break() is executed by clone. 
     * The message is returned to parent (this task), 
     * which executes su_clone_report().
     * Then the message is again returned to clone, 
     * which executes su_clone_report2() and exits.
     */
    sc->sc_wait = &one;
    su_msg_send(rclone);

    su_root_step(root, 0);
    su_root_step(root, 0);

    while (one)
      su_root_step(root, 10);

#if SU_HAVE_PTHREADS
    if (!pthread_equal(clone_tid, pthread_self()))
      pthread_join(clone_tid, NULL);
#endif
  }
}

#if SU_HAVE_PTHREADS		/* No-op without threads */
static
void su_clone_paused(su_root_magic_t *magic, su_msg_r msg, su_msg_arg_t *arg)
{
  su_cloned_t *cloned = *(su_cloned_t **)arg;
  assert(cloned);
  pthread_cond_wait(cloned->sc_resume, cloned->sc_pause);
}
#endif

/** Pause a clone.
 *
 * Obtain a exclusive lock on clone's private data.
 *
 * @retval 0 if successful (and clone is paused)
 * @retval -1 upon an error
 */
int su_clone_pause(su_clone_r rclone)
{
#if SU_HAVE_PTHREADS		/* No-op without threads */
  su_cloned_t *cloned = su_msg_data(rclone);
  su_msg_r m = SU_MSG_R_INIT;

  if (!cloned)
    return (errno = EFAULT), -1;

  if (pthread_equal(pthread_self(), cloned->sc_tid))
    return 0;

  if (su_msg_create(m, su_clone_task(rclone), su_task_null,
		    su_clone_paused, sizeof cloned) < 0)
    return -1;

  *(su_cloned_t **)su_msg_data(m) = cloned;

  if (su_msg_send(m) < 0)
    return -1;

  if (pthread_mutex_lock(cloned->sc_pause) < 0)
    return -1;
  pthread_cond_signal(cloned->sc_resume);
#endif

  return 0;
}

/** Resume a clone.
 *
 * Give up a exclusive lock on clone's private data.
 *
 * @retval 0 if successful (and clone is resumed)
 * @retval -1 upon an error
 */
int su_clone_resume(su_clone_r rclone)
{
#if SU_HAVE_PTHREADS		/* No-op without threads */
  su_cloned_t *cloned = su_msg_data(rclone);

  if (!cloned)
    return (errno = EFAULT), -1;

  if (pthread_equal(pthread_self(), cloned->sc_tid))
    return 0;

  if (pthread_mutex_unlock(cloned->sc_pause) < 0)
    return -1;
#endif

  return 0;
}


/* =========================================================================
 * Messages
 */

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
  su_port_t *port = to->sut_port;
  su_msg_t *msg;

  SU_PORT_LOCK(port, su_msg_create);
  msg = su_zalloc(NULL /*port->sup_home*/, sizeof(*msg) + size);
  SU_PORT_UNLOCK(port, su_msg_create);

  if (msg) {
    msg->sum_size = sizeof(*msg) + size;
    SU_TASK_COPY(msg->sum_to, to, su_msg_create);
    SU_TASK_COPY(msg->sum_from, from, su_msg_create);
    msg->sum_func = wakeup;
    *rmsg = msg;
    return 0;
  } 

  *rmsg = NULL;
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

/**
 * Allocates a reply message of given size.
 *
 * @param reply     handle to the new message (may be uninitialized prior calling)
 * @param msg       the incoming message
 * @param wakeup    function that is called when message is delivered
 * @param size      size of the message data
 *
 * @retval 0 if successful,
 * @retval -1 otherwise.
 */

int su_msg_reply(su_msg_r reply, su_msg_r const msg,
		 su_msg_f wakeup, isize_t size)
{
  su_msg_r msg0;

  assert(msg != reply);

  *msg0 = *msg;
  *reply = NULL;

  return su_msg_create(reply, su_msg_from(msg0), su_msg_to(msg0), wakeup, size);
}


/** Send a delivery report.
 *
 * If the sender has attached a delivery report function to message with
 * su_msg_report(), the message is returned to the message queue of the
 * sending task. The sending task calls the delivery report function when it
 * has received the message.
 */
void su_msg_delivery_report(su_msg_r msg)
{
  su_task_r swap;

  if (!msg || !msg[0])
    return;

  if (!msg[0]->sum_report) {
    su_msg_destroy(msg);
    return;
  }

  *swap = *msg[0]->sum_from;
  *msg[0]->sum_from = *msg[0]->sum_to;
  *msg[0]->sum_to = *swap;

  msg[0]->sum_func = msg[0]->sum_report;
  msg[0]->sum_report = NULL;
  su_msg_send(msg);
}

/** Save a message. */
void su_msg_save(su_msg_r save, su_msg_r msg)
{
  if (save) {
    if (msg)
      save[0] = msg[0];
    else
      save[0] = NULL;
  }
  if (msg)
    msg[0] = NULL;
}

/**
 * Destroys an unsent message.
 *
 * @param rmsg       message handle.
 */
void su_msg_destroy(su_msg_r rmsg)
{
  assert(rmsg);

  if (rmsg[0]) {
    /* su_port_t *port = rmsg[0]->sum_to->sut_port; */

    /* SU_PORT_INCREF(port, su_msg_destroy); */
    SU_TASK_ZAP(rmsg[0]->sum_to, su_msg_destroy);
    SU_TASK_ZAP(rmsg[0]->sum_from, su_msg_destroy);

    su_free(NULL /* port->sup_home */, rmsg[0]);
    /* SU_PORT_UNLOCK(port, su_msg_destroy); */

    /* SU_PORT_DECREF(port, su_msg_destroy); */
  }

  rmsg[0] = NULL;
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
 * @param msg       message handle
 *
 * @return The task handle of the sender is returned.  
 */
_su_task_r su_msg_from(su_msg_r const msg)
{
  return msg[0] ? msg[0]->sum_from : NULL;
}

/** Get destination task.
 *
 * The function @c su_msg_from returns the task handle belonging to the
 * recipient of the message.
 *
 * If the message handle contains NULL the function @c su_msg_to
 * returns NULL.
 *
 * @param msg       message handle
 *
 * @return The task handle of the recipient is returned.  
 */
_su_task_r su_msg_to(su_msg_r const msg)
{
  return msg[0] ? msg[0]->sum_to : NULL;
}

/** Remove references to 'from' and 'to' tasks from a message. 
 *
 * @param msg       message handle
 */
void su_msg_remove_refs(su_msg_r const msg)
{
  if (msg[0]) {
    su_task_deinit(msg[0]->sum_to);
    su_task_deinit(msg[0]->sum_from);
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
    assert(msg->sum_to->sut_port);
    return su_port_send(msg->sum_to->sut_port, rmsg);
  }

  return 0;		
}
