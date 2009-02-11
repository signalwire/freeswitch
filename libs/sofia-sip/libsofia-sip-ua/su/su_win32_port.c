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
 * @CFILE su_win32_port.c
 *
 * Port implementation using WSAEVENTs. Incomplete.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Mon Feb  5 20:29:21 2007 ppessi
 * @date Original: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#define su_port_s su_wsaevent_port_s

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

/** Port based on su_wait() aka WSAWaitForMultipleEvents. */

#define INDEX_MAX (64)

struct su_wsaevent_port_s {
  su_socket_port_t sup_base[1];

#define sup_home sup_base->sup_base->sup_base->sup_home

  unsigned         sup_multishot; /**< Multishot operation? */

  unsigned         sup_registers; /** Counter incremented by
				      su_port_register() or
				      su_port_unregister()
				   */

  int              sup_n_waits; /**< Active su_wait_t in su_waits */
  int              sup_size_waits; /**< Size of allocated su_waits */
  int              sup_pri_offset; /**< Offset to prioritized waits */

  /** Indices from index returned by su_root_register() to tables below.
   *
   * Free elements are negative. Free elements form a list, value of free
   * element is (0 - index of next free element).
   *
   * First element sup_indices[0] points to first free element.
   */
  int             *sup_indices;

  int             *sup_reverses; /** Reverse index */
  su_wakeup_f     *sup_wait_cbs;
  su_wakeup_arg_t**sup_wait_args;
  su_root_t      **sup_wait_roots;
  su_wait_t       *sup_waits;

};

static void su_wsevent_port_decref(su_port_t *, int blocking, char const *who);

static int su_wsevent_port_register(su_port_t *self,
				 su_root_t *root,
				 su_wait_t *wait,
				 su_wakeup_f callback,
				 su_wakeup_arg_t *arg,
				 int priority);
static int su_wsevent_port_unregister(su_port_t *port,
				   su_root_t *root,
				   su_wait_t *wait,
				   su_wakeup_f callback,
				   su_wakeup_arg_t *arg);
static int su_wsevent_port_deregister(su_port_t *self, int i);
static int su_wsevent_port_unregister_all(su_port_t *self, su_root_t *root);
static int su_wsevent_port_eventmask(su_port_t *self,
				  int index,
				  int socket,
				  int events);
static int su_wsevent_port_multishot(su_port_t *self, int multishot);
static int su_wsevent_port_wait_events(su_port_t *self, su_duration_t tout);
static char const *su_wsevent_port_name(su_port_t const *self);

su_port_vtable_t const su_wsevent_port_vtable[1] =
  {{
      /* su_vtable_size: */ sizeof su_wsevent_port_vtable,
      su_pthread_port_lock,
      su_pthread_port_unlock,
      su_base_port_incref,
      su_wsevent_port_decref,
      su_base_port_gsource,
      su_base_port_send,
      su_wsevent_port_register,
      su_wsevent_port_unregister,
      su_wsevent_port_deregister,
      su_wsevent_port_unregister_all,
      su_wsevent_port_eventmask,
      su_base_port_run,
      su_base_port_break,
      su_base_port_step,
      su_pthread_port_thread,
      su_base_port_add_prepoll,
      su_base_port_remove_prepoll,
      su_base_port_timers,
      su_wsevent_port_multishot,
      su_wsevent_port_wait_events,
      su_base_port_getmsgs,
      su_base_port_getmsgs_from,
      su_wsevent_port_name,
      su_base_port_start_shared,
      su_pthread_port_wait,
      su_pthread_port_execute,
      su_base_port_deferrable,
      su_base_port_max_defer,
      su_socket_port_wakeup,
      su_base_port_is_running,
    }};

static char const *su_wsevent_port_name(su_port_t const *self)
{
  return "poll";
}

static void su_wsevent_port_deinit(void *arg)
{
  su_port_t *self = arg;

  SU_DEBUG_9(("%s(%p) called\n", "su_wsevent_port_deinit", self));

  su_socket_port_deinit(self->sup_base);
}

static void su_wsevent_port_decref(su_port_t *self, int blocking, char const *who)
{
  su_base_port_decref(self, blocking, who);
}

/** @internal
 *
 *  Register a @c su_wait_t object. The wait object, a callback function and
 *  an argument pointer is stored in the port object.  The callback function
 *  will be called when the wait object is signaled.
 *
 *  Please note if identical wait objects are inserted, only first one is
 *  ever signalled.
 *
 * @param self	     pointer to port
 * @param root	     pointer to root object
 * @param waits	     pointer to wait object
 * @param callback   callback function pointer
 * @param arg	     argument given to callback function when it is invoked
 * @param priority   relative priority of the wait object
 *              (0 is normal, 1 important, 2 realtime)
 *
 * @return
 *   Positive index of the wait object,
 *   or -1 upon an error.
 */
int su_wsevent_port_register(su_port_t *self,
			  su_root_t *root,
			  su_wait_t *wait,
			  su_wakeup_f callback,
			  su_wakeup_arg_t *arg,
			  int priority)
{
  int i, j, n;

  assert(su_port_own_thread(self));

  n = self->sup_n_waits;

  if (n >= SU_WAIT_MAX)
    return su_seterrno(ENOMEM);

  if (n >= self->sup_size_waits) {
    su_home_t *h = self->sup_home;
    /* Reallocate size arrays */
    int size;
    int *indices;
    int *reverses;
    su_wait_t *waits;
    su_wakeup_f *wait_cbs;
    su_wakeup_arg_t **wait_args;
    su_root_t **wait_tasks;

    if (self->sup_size_waits == 0)
      size = su_root_size_hint;
    else
      size = 2 * self->sup_size_waits;

    if (size < SU_WAIT_MIN)
      size = SU_WAIT_MIN;

    /* Too large */
    if (-3 - size > 0)
      return (errno = ENOMEM), -1;

    indices = su_realloc(h, self->sup_indices, (size + 1) * sizeof(*indices));
    if (indices) {
      self->sup_indices = indices;

      if (self->sup_size_waits == 0)
	indices[0] = -1;

      for (i = self->sup_size_waits + 1; i <= size; i++)
	indices[i] = -1 - i;
    }

    reverses = su_realloc(h, self->sup_reverses, size * sizeof(*waits));
    if (reverses) {
      for (i = self->sup_size_waits; i < size; i++)
	reverses[i] = -1;
      self->sup_reverses = reverses;
    }

    waits = su_realloc(h, self->sup_waits, size * sizeof(*waits));
    if (waits)
      self->sup_waits = waits;

    wait_cbs = su_realloc(h, self->sup_wait_cbs, size * sizeof(*wait_cbs));
    if (wait_cbs)
      self->sup_wait_cbs = wait_cbs;

    wait_args = su_realloc(h, self->sup_wait_args, size * sizeof(*wait_args));
    if (wait_args)
      self->sup_wait_args = wait_args;

    /* Add sup_wait_roots array, if needed */
    wait_tasks = su_realloc(h, self->sup_wait_roots, size * sizeof(*wait_tasks));
    if (wait_tasks)
      self->sup_wait_roots = wait_tasks;

    if (!(indices &&
	  reverses && waits && wait_cbs && wait_args && wait_tasks)) {
      return -1;
    }

    self->sup_size_waits = size;
  }

  i = -self->sup_indices[0]; assert(i <= self->sup_size_waits);

  if (priority > 0) {
    /* Insert */
    for (n = self->sup_n_waits; n > 0; n--) {
      j = self->sup_reverses[n-1]; assert(self->sup_indices[j] == n - 1);
      self->sup_indices[j] = n;
      self->sup_reverses[n] = j;
      self->sup_waits[n] = self->sup_waits[n-1];
      self->sup_wait_cbs[n] = self->sup_wait_cbs[n-1];
      self->sup_wait_args[n] = self->sup_wait_args[n-1];
      self->sup_wait_roots[n] = self->sup_wait_roots[n-1];
    }

    self->sup_pri_offset++;
  }
  else {
    /* Append - no need to move anything */
    n = self->sup_n_waits;
  }

  self->sup_n_waits++;

  self->sup_indices[0] = self->sup_indices[i];  /* Free index */
  self->sup_indices[i] = n;

  self->sup_reverses[n] = i;
  self->sup_waits[n] = *wait;
  self->sup_wait_cbs[n] = callback;
  self->sup_wait_args[n] = arg;
  self->sup_wait_roots[n] = root;

  self->sup_registers++;

  /* Just like epoll, we return -1 or positive integer */

  return i;
}

/** Deregister a su_wait_t object. */
static int su_wsevent_port_deregister0(su_port_t *self, int i, int destroy_wait)
{
  int n, N, *indices, *reverses;

  indices = self->sup_indices;
  reverses = self->sup_reverses;

  n = indices[i]; assert(n >= 0);

  if (destroy_wait)
    su_wait_destroy(&self->sup_waits[n]);

  N = --self->sup_n_waits;

  if (n < self->sup_pri_offset) {
    int j = --self->sup_pri_offset;
    if (n != j) {
      assert(reverses[j] > 0);
      assert(indices[reverses[j]] == j);
      indices[reverses[j]] = n;
      reverses[n] = reverses[j];

      self->sup_waits[n] = self->sup_waits[j];
      self->sup_wait_cbs[n] = self->sup_wait_cbs[j];
      self->sup_wait_args[n] = self->sup_wait_args[j];
      self->sup_wait_roots[n] = self->sup_wait_roots[j];
      n = j;
    }
  }

  if (n < N) {
    assert(reverses[N] > 0);
    assert(indices[reverses[N]] == N);

    indices[reverses[N]] = n;
    reverses[n] = reverses[N];

    self->sup_waits[n] = self->sup_waits[N];
    self->sup_wait_cbs[n] = self->sup_wait_cbs[N];
    self->sup_wait_args[n] = self->sup_wait_args[N];
    self->sup_wait_roots[n] = self->sup_wait_roots[N];
    n = N;
  }

  reverses[n] = -1;
  memset(&self->sup_waits[n], 0, sizeof self->sup_waits[n]);
  self->sup_wait_cbs[n] = NULL;
  self->sup_wait_args[n] = NULL;
  self->sup_wait_roots[n] = NULL;

  indices[i] = indices[0];
  indices[0] = -i;

  self->sup_registers++;

  return i;
}


/** Unregister a su_wait_t object.
 *
 * Unregisters a su_wait_t object. The wait object, a callback function and
 * a argument are removed from the port object.
 *
 * @param self     - pointer to port object
 * @param root     - pointer to root object
 * @param wait     - pointer to wait object
 * @param callback - callback function pointer (may be NULL)
 * @param arg      - argument given to callback function when it is invoked
 *                   (may be NULL)
 *
 * @deprecated Use su_wsevent_port_deregister() instead.
 *
 * @return Nonzero index of the wait object, or -1 upon an error.
 */
int su_wsevent_port_unregister(su_port_t *self,
			    su_root_t *root,
			    su_wait_t *wait,
			    su_wakeup_f callback, /* XXX - ignored */
			    su_wakeup_arg_t *arg)
{
  int n, N;

  assert(self);
  assert(su_port_own_thread(self));

  N = self->sup_n_waits;

  for (n = 0; n < N; n++) {
    if (SU_WAIT_CMP(wait[0], self->sup_waits[n]) == 0) {
      return su_wsevent_port_deregister0(self, self->sup_reverses[n], 0);
    }
  }

  su_seterrno(ENOENT);

  return -1;
}

/** Deregister a su_wait_t object.
 *
 *  Deregisters a registration by index. The wait object, a callback
 *  function and a argument are removed from the port object. The wait
 *  object is destroyed.
 *
 * @param self     - pointer to port object
 * @param i        - registration index
 *
 * @return Index of the wait object, or -1 upon an error.
 */
int su_wsevent_port_deregister(su_port_t *self, int i)
{
  su_wait_t wait[1] = { SU_WAIT_INIT };
  int retval;

  assert(self);
  assert(su_port_own_thread(self));

  if (i <= 0 || i > self->sup_size_waits)
    return su_seterrno(EBADF);

  if (self->sup_indices[i] < 0)
    return su_seterrno(EBADF);

  retval = su_wsevent_port_deregister0(self, i, 1);

  su_wait_destroy(wait);

  return retval;
}


/** @internal
 * Unregister all su_wait_t objects.
 *
 * Unregisters all su_wait_t objects and destroys all queued timers
 * associated with given root object.
 *
 * @param  self     - pointer to port object
 * @param  root     - pointer to root object
 *
 * @return Number of wait objects removed.
 */
int su_wsevent_port_unregister_all(su_port_t *self,
				su_root_t *root)
{
  int i, j, index, N;
  int             *indices, *reverses;
  su_wait_t       *waits;
  su_wakeup_f     *wait_cbs;
  su_wakeup_arg_t**wait_args;
  su_root_t      **wait_roots;

  assert(su_port_own_thread(self));

  N          = self->sup_n_waits;
  indices    = self->sup_indices;
  reverses   = self->sup_reverses;
  waits      = self->sup_waits;
  wait_cbs   = self->sup_wait_cbs;
  wait_args  = self->sup_wait_args;
  wait_roots = self->sup_wait_roots;

  for (i = j = 0; i < N; i++) {
    index = reverses[i]; assert(index > 0 && indices[index] == i);

    if (wait_roots[i] == root) {
      /* XXX - we should free all resources associated with this, too */
      if (i < self->sup_pri_offset)
	self->sup_pri_offset--;

      indices[index] = indices[0];
      indices[0] = -index;
      continue;
    }

    if (i != j) {
      indices[index] = j;
      reverses[j]   = reverses[i];
      waits[j]      = waits[i];
      wait_cbs[j]   = wait_cbs[i];
      wait_args[j]  = wait_args[i];
      wait_roots[j] = wait_roots[i];
    }

    j++;
  }

  for (i = j; i < N; i++) {
    reverses[i] = -1;
    wait_cbs[i] = NULL;
    wait_args[i] = NULL;
    wait_roots[i] = NULL;
  }
  memset(&waits[j], 0, (char *)&waits[N] - (char *)&waits[j]);

  self->sup_n_waits = j;
  self->sup_registers++;

  return N - j;
}

/**Set mask for a registered event. @internal
 *
 * Sets the mask describing events that can signal the registered callback.
 *
 * @param port   pointer to port object
 * @param index  registration index
 * @param socket socket
 * @param events new event mask
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
int su_wsevent_port_eventmask(su_port_t *self, int index, int socket, int events)
{
  int n;
  assert(self);
  assert(su_port_own_thread(self));

  if (index <= 0 || index > self->sup_size_waits)
    return su_seterrno(EBADF);
  n = self->sup_indices[index];
  if (n < 0)
    return su_seterrno(EBADF);

  return su_wait_mask(&self->sup_waits[n], socket, events);
}

/** @internal Enable multishot mode.
 *
 * Enables, disables or queries the multishot mode for the port. The
 * multishot mode determines how the events are scheduled by port. If
 * multishot mode is enabled, port serves all the sockets that have received
 * network events. If it is disabled, only first socket event is served.
 *
 * @param self      pointer to port object
 * @param multishot multishot mode (0 => disables, 1 => enables, -1 => query)
 *
 * @retval 0 multishot mode is disabled
 * @retval 1 multishot mode is enabled
 * @retval -1 an error occurred
 */
static
int su_wsevent_port_multishot(su_port_t *self, int multishot)
{
  if (multishot < 0)
    return self->sup_multishot;
  else if (multishot == 0 || multishot == 1)
    return self->sup_multishot = multishot;
  else
    return (errno = EINVAL), -1;
}


/** @internal
 * Wait (poll()) for wait objects in port.
 *
 * @param self     pointer to port
 * @param tout     timeout in milliseconds
 *
 * @return number of events handled
 */
static
int su_wsevent_port_wait_events(su_port_t *self, su_duration_t tout)
{
  int i, events = 0;
  su_wait_t *waits = self->sup_waits;
  int n = self->sup_n_waits;
  su_root_t *root;

  i = su_wait(waits, (unsigned)n, tout);

  if (i >= 0 && i < n) {
#if 0
    /* poll() can return events for multiple wait objects */
    if (self->sup_multishot) {
      unsigned version = self->sup_registers;

      for (; i < n; i++) {
        if (waits[i].revents) {
          root = self->sup_wait_roots[i];
          self->sup_wait_cbs[i](root ? su_root_magic(root) : NULL,
                                &waits[i],
                                self->sup_wait_args[i]);
          events++;
          /* Callback function used su_register()/su_deregister() */
          if (version != self->sup_registers)
            break;
        }
      }
    }
#else
    if (0) ;
#endif
    else {
      root = self->sup_wait_roots[i];
      self->sup_wait_cbs[i](root ? su_root_magic(root) : NULL,
                            &self->sup_waits[i],
                            self->sup_wait_args[i]);
      events++;
    }
  }

  return events;
}

/** Create a port using WSAEVENTs and WSAWaitForMultipleEvents. */
su_port_t *su_wsaevent_port_create(void)
{
  su_port_t *self = su_home_new(sizeof *self);

  if (!self)
    return self;

  if (su_home_destructor(su_port_home(self), su_wsevent_port_deinit) < 0)
    return su_home_unref(su_port_home(self)), NULL;

  self->sup_multishot = SU_ENABLE_MULTISHOT_POLL;

  if (su_socket_port_init(self->sup_base, su_wsevent_port_vtable) < 0)
    return su_home_unref(su_port_home(self)), NULL;

  return self;
}

int su_wsaevent_clone_start(su_root_t *parent,
			    su_clone_r return_clone,
			    su_root_magic_t *magic,
			    su_root_init_f init,
			    su_root_deinit_f deinit)
{
  return su_pthreaded_port_start(su_wsaevent_port_create,
				 parent, return_clone, magic, init, deinit);
}

