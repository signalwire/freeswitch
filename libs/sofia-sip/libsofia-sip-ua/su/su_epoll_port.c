/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005, 2006, 2007 Nokia Corporation.
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
 * @CFILE su_epoll_port.c
 *
 * Port implementation using epoll(7)
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Fri Jan 26 20:44:14 2007 ppessi
 * @date Original: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#define su_port_s su_epoll_port_s

#include "su_port.h"

#if HAVE_EPOLL

#include "sofia-sip/su.h"
#include "sofia-sip/su_alloc.h"

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <sys/epoll.h>

#define POLL2EPOLL_NEEDED \
  (POLLIN != EPOLLIN || POLLOUT != EPOLLOUT || POLLPRI != EPOLLPRI || \
   POLLERR != EPOLLERR || POLLHUP != EPOLLHUP)

#define POLL2EPOLL(e) (e & (POLLIN|POLLOUT|POLLPRI|POLLERR|POLLHUP))
#define EPOLL2POLL(e) (e & (POLLIN|POLLOUT|POLLPRI|POLLERR|POLLHUP))

/** Port based on epoll(). */

struct su_epoll_port_s {
  su_socket_port_t sup_base[1];

  /** epoll fd */
  int              sup_epoll;
  unsigned         sup_multishot; /**< Multishot operation? */

  unsigned         sup_registers; /** Counter incremented by
				      su_port_register() or
				      su_port_unregister()
				   */
  int              sup_n_registrations;
  int              sup_max_index; /**< Indexes are equal or smaller than this */
  int              sup_size_indices; /**< Size of allocated index table */

  /** Structure containing registration data */
  struct su_epoll_register {
    struct su_epoll_register *ser_next; /* Next in free list */
    su_wakeup_f     ser_cb;
    su_wakeup_arg_t*ser_arg;
    su_root_t      *ser_root;
    int             ser_id; /** registration identifier */
    su_wait_t       ser_wait[1];
  } **sup_indices;
};

static void su_epoll_port_decref(su_port_t *self,
				 int blocking,
				 char const *who);
static int su_epoll_port_register(su_port_t *self,
				  su_root_t *root,
				  su_wait_t *wait,
				  su_wakeup_f callback,
				  su_wakeup_arg_t *arg,
				  int priority);
static int su_epoll_port_unregister(su_port_t *port,
				    su_root_t *root,
				    su_wait_t *wait,
				    su_wakeup_f callback,
				    su_wakeup_arg_t *arg);
static int su_epoll_port_deregister(su_port_t *self, int i);
static int su_epoll_port_unregister_all(su_port_t *self, su_root_t *root);
static int su_epoll_port_eventmask(su_port_t *self,
				   int index,
				   int socket,
				   int events);
static int su_epoll_port_multishot(su_port_t *self, int multishot);
static int su_epoll_port_wait_events(su_port_t *self, su_duration_t tout);
static char const *su_epoll_port_name(su_port_t const *self);

su_port_vtable_t const su_epoll_port_vtable[1] =
  {{
      /* su_vtable_size: */ sizeof su_epoll_port_vtable,
      su_pthread_port_lock,
      su_pthread_port_unlock,
      su_base_port_incref,
      su_epoll_port_decref,
      su_base_port_gsource,
      su_base_port_send,
      su_epoll_port_register,
      su_epoll_port_unregister,
      su_epoll_port_deregister,
      su_epoll_port_unregister_all,
      su_epoll_port_eventmask,
      su_base_port_run,
      su_base_port_break,
      su_base_port_step,
      su_pthread_port_thread,
      su_base_port_add_prepoll,
      su_base_port_remove_prepoll,
      su_base_port_timers,
      su_epoll_port_multishot,
      su_epoll_port_wait_events,
      su_base_port_getmsgs,
      su_base_port_getmsgs_from,
      su_epoll_port_name,
      su_base_port_start_shared,
      su_pthread_port_wait,
      su_pthread_port_execute,
      su_base_port_deferrable,
      su_base_port_max_defer,
      su_socket_port_wakeup,
      su_base_port_is_running,
    }};

static char const *su_epoll_port_name(su_port_t const *self)
{
  return "epoll";
}

static void su_epoll_port_decref(su_port_t *self, int blocking, char const *who)
{
  (void)su_base_port_decref(self, blocking, who);
}

static void su_epoll_port_deinit(void *arg)
{
  su_port_t *self = arg;

  SU_DEBUG_9(("%s(%p) called\n", "su_epoll_port_deinit", (void* )self));

  su_socket_port_deinit(self->sup_base);

  close(self->sup_epoll), self->sup_epoll = -1;
}

/** @internal
 *
 *  Register a #su_wait_t object. The wait object, a callback function and
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
int su_epoll_port_register(su_port_t *self,
			   su_root_t *root,
			   su_wait_t *wait,
			   su_wakeup_f callback,
			   su_wakeup_arg_t *arg,
			   int priority)
{
  int i, j, n;
  struct epoll_event ev;
  struct su_epoll_register *ser;
  struct su_epoll_register **indices = self->sup_indices;

  assert(su_port_own_thread(self));

  n = self->sup_size_indices;

  if (n >= SU_WAIT_MAX)
    return su_seterrno(ENOMEM);

  ser = indices[0];

  if (!ser) {
    su_home_t *h = su_port_home(self);

    i = self->sup_max_index, j = i == 0 ? 15 : i + 16;

    if (j >= self->sup_size_indices) {
      /* Reallocate index table */
      n = n < 1024 ? 2 * n : n + 1024;
      indices = su_realloc(h, indices, n * sizeof(indices[0]));
      if (!indices)
	return -1;
      self->sup_indices = indices;
      self->sup_size_indices = n;
    }

    /* Allocate registrations */
    ser = su_zalloc(h, (j - i) * (sizeof *ser));
    if (!ser)
      return -1;

    indices[0] = ser;

    for (i++; i <= j; i++) {
      ser->ser_id = i;
      ser->ser_next = i < j ? ser + 1 : NULL;
      indices[i] = ser++;
    }

    self->sup_max_index = j;

    ser = indices[0];
  }

  i = ser->ser_id;

  ev.events = POLL2EPOLL(wait->events);
  ev.data.u64 = 0;
  ev.data.u32 = (uint32_t)i;

  if (epoll_ctl(self->sup_epoll, EPOLL_CTL_ADD, wait->fd, &ev) == -1) {
    SU_DEBUG_0(("EPOLL_CTL_ADD(%u, %u) failed: %s\n",
		wait->fd, ev.events, strerror(errno)));
    return -1;
  }

  indices[0] = ser->ser_next;

  ser->ser_next = NULL;
  *ser->ser_wait = *wait;
  ser->ser_cb = callback;
  ser->ser_arg = arg;
  ser->ser_root = root;

  self->sup_registers++;
  self->sup_n_registrations++;

  return i;			/* return index */
}

/** Deregister a su_wait_t object. */
static int su_epoll_port_deregister0(su_port_t *self, int i, int destroy_wait)
{
  struct su_epoll_register **indices = self->sup_indices;
  struct su_epoll_register *ser;

  ser = self->sup_indices[i];
  if (ser == NULL || ser->ser_cb == NULL) {
    su_seterrno(ENOENT);
    return -1;
  }

  assert(ser->ser_id == i);

  if (epoll_ctl(self->sup_epoll, EPOLL_CTL_DEL, ser->ser_wait->fd, NULL) == -1) {
    SU_DEBUG_1(("su_port(%p): EPOLL_CTL_DEL(%u): %s\n", (void *)self,
		ser->ser_wait->fd, su_strerror(su_errno())));
  }

  if (destroy_wait)
    su_wait_destroy(ser->ser_wait);

  memset(ser, 0, sizeof *ser);
  ser->ser_id = i;
  ser->ser_next = indices[0], indices[0] = ser;

  self->sup_n_registrations--;
  self->sup_registers++;

  return i;
}


/** Unregister a su_wait_t object.
 *
 *  The function su_epoll_port_unregister() unregisters a su_wait_t object. The
 *  wait object, a callback function and a argument are removed from the
 *  port object.
 *
 * @param self     - pointer to port object
 * @param root     - pointer to root object
 * @param wait     - pointer to wait object
 * @param callback - callback function pointer (may be NULL)
 * @param arg      - argument given to callback function when it is invoked
 *                   (may be NULL)
 *
 * @deprecated Use su_epoll_port_deregister() instead.
 *
 * @return Nonzero index of the wait object, or -1 upon an error.
 */
int su_epoll_port_unregister(su_port_t *self,
			     su_root_t *root,
			     su_wait_t *wait,
			     su_wakeup_f callback, /* XXX - ignored */
			     su_wakeup_arg_t *arg)
{
  int i, I;

  struct su_epoll_register *ser;

  assert(self);
  assert(su_port_own_thread(self));

  I = self->sup_max_index;

  for (i = 1; i <= I; i++) {
    ser = self->sup_indices[i];

    if (ser->ser_cb &&
	arg == ser->ser_arg &&
	SU_WAIT_CMP(wait[0], ser->ser_wait[0]) == 0)
      return su_epoll_port_deregister0(self, ser->ser_id, 0);
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
int su_epoll_port_deregister(su_port_t *self, int i)
{
  struct su_epoll_register *ser;

  if (i <= 0 || i > self->sup_max_index)
    return su_seterrno(EBADF);

  ser = self->sup_indices[i];
  if (!ser->ser_cb)
    return su_seterrno(EBADF);

  return su_epoll_port_deregister0(self, i, 1);
}


/** @internal
 * Unregister all su_wait_t objects of given su_root_t instance.
 *
 * The function su_epoll_port_unregister_all() unregisters all su_wait_t
 * objects associated with given root object.
 *
 * @param  self     - pointer to port object
 * @param  root     - pointer to root object
 *
 * @return Number of wait objects removed.
 */
int su_epoll_port_unregister_all(su_port_t *self, su_root_t *root)
{
  int i, I, n;

  struct su_epoll_register *ser;

  assert(self); assert(root);
  assert(su_port_own_thread(self));

  I = self->sup_max_index;

  for (i = 1, n = 0; i <= I; i++) {
    ser = self->sup_indices[i];
    if (ser->ser_root != root)
      continue;
    su_epoll_port_deregister0(self, ser->ser_id, 0);
    n++;
  }

  return n;
}

/**Set mask for a registered event. @internal
 *
 * The function su_epoll_port_eventmask() sets the mask describing events that can
 * signal the registered callback.
 *
 * @param port   pointer to port object
 * @param index  registration index
 * @param socket socket
 * @param events new event mask
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
int su_epoll_port_eventmask(su_port_t *self, int index, int socket, int events)
{
  struct su_epoll_register *ser;
  struct epoll_event ev;

  if (index <= 0 || index > self->sup_max_index)
    return su_seterrno(EBADF);

  ser = self->sup_indices[index];
  if (!ser->ser_cb)
    return su_seterrno(EBADF);

  ser->ser_wait->events = events;

  ev.events = POLL2EPOLL(events);
  ev.data.u64 = (uint64_t)0;
  ev.data.u32 = (uint32_t)index;

  if (epoll_ctl(self->sup_epoll, EPOLL_CTL_MOD, socket, &ev) == -1) {
    SU_DEBUG_1(("su_port(%p): EPOLL_CTL_MOD(%u): %s\n", (void *)self,
		socket, su_strerror(su_errno())));
    return -1;
  }

  return 0;
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
int su_epoll_port_multishot(su_port_t *self, int multishot)
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
int su_epoll_port_wait_events(su_port_t *self, su_duration_t tout)
{
  int j, n, events = 0, index;
  unsigned version = self->sup_registers;

  int const M = 4;
  struct epoll_event ev[M];

  n = epoll_wait(self->sup_epoll, ev, self->sup_multishot ? M : 1, tout);

  assert(n <= M);

  for (j = 0; j < n; j++) {
    struct su_epoll_register *ser;
    su_root_magic_t *magic;

    index = (int)ev[j].data.u32;
    if (!ev[j].events || index <= 0 || self->sup_max_index < index)
      continue;
    ser = self->sup_indices[index];

    magic = ser->ser_root ? su_root_magic(ser->ser_root) : NULL;
    ser->ser_wait->revents = ev[j].events;
    ser->ser_cb(magic, ser->ser_wait, ser->ser_arg);
    events++;
    if (version != self->sup_registers)
      /* Callback function used su_register()/su_deregister() */
      return events;
  }

  return n;
}

/** Create a port using epoll() or poll().
 */
su_port_t *su_epoll_port_create(void)
{
  su_port_t *self;
  int epoll = epoll_create(su_root_size_hint);

  if (epoll == -1) {
    /* Fallback to poll() */
    SU_DEBUG_3(("%s(): epoll_create() => %u: %s\n",
		"su_port_create", epoll, strerror(errno)));
    return su_poll_port_create();
  }

  self = su_home_new(sizeof *self);
  if (!self) {
    close(epoll);
    return self;
  }

  SU_DEBUG_9(("%s(%p): epoll_create() => %u: %s\n",
	      "su_port_create", (void *)self, self->sup_epoll, "OK"));

  if (su_home_destructor(su_port_home(self), su_epoll_port_deinit) < 0 ||
      !(self->sup_indices =
	su_zalloc(su_port_home(self),
		  (sizeof self->sup_indices[0]) *
		  (self->sup_size_indices = 64)))) {
    su_home_unref(su_port_home(self));
    close(epoll);
    return NULL;
  }

  self->sup_epoll = epoll;
  self->sup_multishot = SU_ENABLE_MULTISHOT_POLL;

  if (su_socket_port_init(self->sup_base, su_epoll_port_vtable) < 0)
    return su_home_unref(su_port_home(self)), NULL;

  return self;
}

int su_epoll_clone_start(su_root_t *parent,
			 su_clone_r return_clone,
			 su_root_magic_t *magic,
			 su_root_init_f init,
			 su_root_deinit_f deinit)
{
  return su_pthreaded_port_start(su_epoll_port_create,
				 parent, return_clone, magic, init, deinit);
}

#else

su_port_t *su_epoll_port_create(void)
{
  return su_default_port_create();
}

int su_epoll_clone_start(su_root_t *parent,
			 su_clone_r return_clone,
			 su_root_magic_t *magic,
			 su_root_init_f init,
			 su_root_deinit_f deinit)
{
  return su_default_clone_start(parent, return_clone, magic, init, deinit);
}

#endif  /* HAVE_EPOLL */
