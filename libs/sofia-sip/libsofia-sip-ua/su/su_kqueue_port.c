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
 * @CFILE su_kqueue_port.c
 *
 * Port implementation using kqueue()
 *
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Sun Feb 18 19:55:37 EET 2007 mela
 */

#include "config.h"
#include "sofia-sip/su.h"

#define su_port_s su_kqueue_port_s

#include "su_port.h"

#if HAVE_KQUEUE

#include "sofia-sip/su_alloc.h"

#include <sys/event.h>

#define SU_ENABLE_MULTISHOT_KQUEUE 1

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

/** Port based on kqueue(). */

struct su_kqueue_port_s {
  su_socket_port_t sup_base[1];

#define sup_home sup_base->sup_base->sup_base->sup_home

  /** kqueue fd */
  int              sup_kqueue;
  unsigned         sup_multishot; /**< Multishot operation? */

  unsigned         sup_registers; /** Counter incremented by
				      su_port_register() or
				      su_port_unregister()
				   */
  int              sup_n_registrations;
  int              sup_max_index; /**< Indexes are equal or smaller than this */
  int              sup_size_indices; /**< Size of allocated index table */

#define INDEX_MAX (0x7fffffff)

  /** Structure containing registration data */
  struct su_register {
    struct su_register *ser_next; /* Next in free list */
    su_wakeup_f     ser_cb;
    su_wakeup_arg_t*ser_arg;
    su_root_t      *ser_root;
    int             ser_id; /** registration identifier */
    su_wait_t       ser_wait[1];
  } **sup_indices;
};

static void su_kqueue_port_decref(su_port_t *, int blocking, char const *who);

static int su_kqueue_port_register(su_port_t *self,
				 su_root_t *root,
				 su_wait_t *wait,
				 su_wakeup_f callback,
				 su_wakeup_arg_t *arg,
				 int priority);
static int su_kqueue_port_unregister(su_port_t *port,
				   su_root_t *root,
				   su_wait_t *wait,
				   su_wakeup_f callback,
				   su_wakeup_arg_t *arg);
static int su_kqueue_port_deregister(su_port_t *self, int i);
static int su_kqueue_port_unregister_all(su_port_t *self, su_root_t *root);
static int su_kqueue_port_eventmask(su_port_t *self,
				  int index,
				  int socket,
				  int events);
static int su_kqueue_port_multishot(su_port_t *self, int multishot);
static int su_kqueue_port_wait_events(su_port_t *self, su_duration_t tout);
static char const *su_kqueue_port_name(su_port_t const *self);

su_port_vtable_t const su_kqueue_port_vtable[1] =
  {{
      /* su_vtable_size: */ sizeof su_kqueue_port_vtable,
      su_pthread_port_lock,
      su_pthread_port_unlock,
      su_base_port_incref,
      su_kqueue_port_decref,
      su_base_port_gsource,
      su_base_port_send,
      su_kqueue_port_register,
      su_kqueue_port_unregister,
      su_kqueue_port_deregister,
      su_kqueue_port_unregister_all,
      su_kqueue_port_eventmask,
      su_base_port_run,
      su_base_port_break,
      su_base_port_step,
      su_pthread_port_thread,
      su_base_port_add_prepoll,
      su_base_port_remove_prepoll,
      su_base_port_timers,
      su_kqueue_port_multishot,
      su_kqueue_port_wait_events,
      su_base_port_getmsgs,
      su_base_port_getmsgs_from,
      su_kqueue_port_name,
      su_base_port_start_shared,
      su_pthread_port_wait,
      su_pthread_port_execute,
      su_base_port_deferrable,
      su_base_port_max_defer,
      su_socket_port_wakeup,
      su_base_port_is_running,
    }};

static char const *su_kqueue_port_name(su_port_t const *self)
{
  return "kqueue";
}

static void su_kqueue_port_deinit(void *arg)
{
  su_port_t *self = arg;

  SU_DEBUG_9(("%s(%p) called\n", "su_kqueue_port_deinit", (void *)self));

  su_socket_port_deinit(self->sup_base);

  close(self->sup_kqueue);
}

static void su_kqueue_port_decref(su_port_t *self, int blocking, char const *who)
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
int su_kqueue_port_register(su_port_t *self,
			    su_root_t *root,
			    su_wait_t *wait,
			    su_wakeup_f callback,
			    su_wakeup_arg_t *arg,
			    int priority)
{
  int i, j, n;
  struct su_register *ser;
  struct su_register **indices = self->sup_indices;
  struct kevent ev[1];
  int flags;

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

  flags = (wait->events & SU_WAIT_IN) ? EV_ADD : EV_ADD | EV_DISABLE;
  EV_SET(ev, wait->fd, EVFILT_READ, flags, 0, 0, (void *)(intptr_t)i);
  if (kevent(self->sup_kqueue, ev, 1, NULL, 0, NULL) == -1) {
    SU_DEBUG_0(("kevent((%u, %s, %u, %p)) failed: %s\n",
				wait->fd, "EVFILT_READ", flags, (void *)(intptr_t)i, strerror(errno)));
    return -1;
  }

  flags = (wait->events & SU_WAIT_OUT) ? EV_ADD : EV_ADD | EV_DISABLE;
  EV_SET(ev, wait->fd, EVFILT_WRITE, flags, 0, 0, (void *)(intptr_t)i);
  if (kevent(self->sup_kqueue, ev, 1, NULL, 0, NULL) == -1) {
    int error = errno;
    SU_DEBUG_0(("kevent((%u, %s, %u, %p)) failed: %s\n",
				wait->fd, "EVFILT_WRITE", flags, (void *)(intptr_t)i, strerror(error)));

    EV_SET(ev, wait->fd, EVFILT_READ, EV_DELETE, 0, 0, (void *)(intptr_t)i);
    kevent(self->sup_kqueue, ev, 1, NULL, 0, NULL);

    errno = error;
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
static int su_kqueue_port_deregister0(su_port_t *self, int i, int destroy_wait)
{
  struct su_register **indices = self->sup_indices;
  struct su_register *ser;
  struct kevent ev[1];
  su_wait_t *wait;

  ser = self->sup_indices[i];
  if (ser == NULL || ser->ser_cb == NULL) {
    su_seterrno(ENOENT);
    return -1;
  }

  assert(ser->ser_id == i);

  wait = ser->ser_wait;

  EV_SET(ev, wait->fd, EVFILT_READ, EV_DELETE, 0, 0, (void *)(intptr_t)i);
  if (kevent(self->sup_kqueue, ev, 1, NULL, 0, NULL) == -1) {
    SU_DEBUG_0(("remove kevent((%u, %s, %s, %p)) failed: %s\n",
				wait->fd, "EVFILT_READ", "EV_DELETE", (void *)(intptr_t)i,
		strerror(errno)));
  }

  EV_SET(ev, wait->fd, EVFILT_WRITE, EV_DELETE, 0, 0, (void *)(intptr_t)i);
  if (kevent(self->sup_kqueue, ev, 1, NULL, 0, NULL) == -1) {
    SU_DEBUG_0(("remove kevent((%u, %s, %s, %p)) failed: %s\n",
				wait->fd, "EVFILT_WRITE", "EV_DELETE", (void *)(intptr_t)i,
		strerror(errno)));
  }

  if (destroy_wait)
    su_wait_destroy(wait);

  memset(ser, 0, sizeof *ser);
  ser->ser_id = i;
  ser->ser_next = indices[0], indices[0] = ser;

  self->sup_n_registrations--;
  self->sup_registers++;

  return i;
}


/** Unregister a su_wait_t object.
 *
 *  The function su_kqueue_port_unregister() unregisters a su_wait_t object. The
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
 * @deprecated Use su_kqueue_port_deregister() instead.
 *
 * @return Nonzero index of the wait object, or -1 upon an error.
 */
int su_kqueue_port_unregister(su_port_t *self,
			      su_root_t *root,
			      su_wait_t *wait,
			      su_wakeup_f callback, /* XXX - ignored */
			      su_wakeup_arg_t *arg)
{
  int i, I;

  struct su_register *ser;

  assert(self);
  assert(su_port_own_thread(self));

  I = self->sup_max_index;

  for (i = 1; i <= I; i++) {
    ser = self->sup_indices[i];

    if (ser->ser_cb &&
	arg == ser->ser_arg &&
	SU_WAIT_CMP(wait[0], ser->ser_wait[0]) == 0)
      return su_kqueue_port_deregister0(self, ser->ser_id, 0);
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
int su_kqueue_port_deregister(su_port_t *self, int i)
{
  struct su_register *ser;

  if (i <= 0 || i > self->sup_max_index)
    return su_seterrno(EBADF);

  ser = self->sup_indices[i];
  if (!ser->ser_cb)
    return su_seterrno(EBADF);

  return su_kqueue_port_deregister0(self, i, 1);
}

/** @internal
 * Unregister all su_wait_t objects belonging to a root.
 *
 * The function su_kqueue_port_unregister_all() unregisters all su_wait_t objects
 * and destroys all queued timers associated with given root object.
 *
 * @param  self     - pointer to port object
 * @param  root     - pointer to root object
 *
 * @return Number of wait objects removed.
 */
int su_kqueue_port_unregister_all(su_port_t *self,
				su_root_t *root)
{
  int i, I, n;

  struct su_register *ser;

  assert(self); assert(root);
  assert(su_port_own_thread(self));

  I = self->sup_max_index;

  for (i = 1, n = 0; i <= I; i++) {
    ser = self->sup_indices[i];
    if (ser->ser_root != root)
      continue;
    su_kqueue_port_deregister0(self, ser->ser_id, 0);
    n++;
  }

  return n;
}

/**Set mask for a registered event. @internal
 *
 * The function su_kqueue_port_eventmask() sets the mask describing events
 * that can signal the registered callback.
 *
 * @param port   pointer to port object
 * @param index  registration index
 * @param socket socket
 * @param events new event mask
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
int su_kqueue_port_eventmask(su_port_t *self, int index, int socket, int events)
{
  struct su_register *ser;
  struct kevent ev[1];
  su_wait_t *wait;
  int flags;

  if (index <= 0 || index > self->sup_max_index)
    return su_seterrno(EBADF);

  ser = self->sup_indices[index];
  if (!ser->ser_cb)
    return su_seterrno(EBADF);

  wait = ser->ser_wait;

  assert(socket == wait->fd);

  wait->events = events;

  flags = (wait->events & SU_WAIT_IN) ? EV_ADD | EV_ENABLE : EV_ADD | EV_DISABLE;
  EV_SET(ev, wait->fd, EVFILT_READ, flags, 0, 0, (void *)(intptr_t)index);
  if (kevent(self->sup_kqueue, ev, 1, NULL, 0, NULL) == -1) {
    SU_DEBUG_0(("modify kevent((%u, %s, %s, %p)) failed: %s\n",
		wait->fd, "EVFILT_READ",
		(events & SU_WAIT_IN) ? "EV_ENABLE" : "EV_DISABLE",
				(void *)(intptr_t)index, strerror(errno)));
  }

  flags = (wait->events & SU_WAIT_OUT) ? EV_ADD | EV_ENABLE : EV_ADD | EV_DISABLE;
  EV_SET(ev, wait->fd, EVFILT_WRITE, flags, 0, 0, (void *)(intptr_t)index);
  if (kevent(self->sup_kqueue, ev, 1, NULL, 0, NULL) == -1) {
    SU_DEBUG_0(("modify kevent((%u, %s, %s, %p)) failed: %s\n",
		wait->fd, "EVFILT_WRITE",
		(events & SU_WAIT_OUT) ? "EV_ENABLE" : "EV_DISABLE",
				(void *)(intptr_t)index, strerror(errno)));
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
int su_kqueue_port_multishot(su_port_t *self, int multishot)
{
  if (multishot < 0)
    return self->sup_multishot;
  else if (multishot == 0 || multishot == 1)
    return self->sup_multishot = multishot;
  else
    return (errno = EINVAL), -1;
}


/** @internal
 * Wait (kqueue()) for wait objects in port.
 *
 * @param self     pointer to port
 * @param tout     timeout in milliseconds
 *
 * @return number of events handled
 */
static
int su_kqueue_port_wait_events(su_port_t *self, su_duration_t tout)
{
  int j, n, events = 0, index;
  unsigned version = self->sup_registers;

  int const M = 4;
  struct kevent ev[M];

  struct timespec ts;

  ts.tv_sec = tout / 1000;
  ts.tv_nsec = tout % 1000 * 1000000;

  n = kevent(self->sup_kqueue, NULL, 0,
	     ev, self->sup_multishot ? M : 1,
	     tout < SU_DURATION_MAX ? &ts : NULL);

  assert(n <= M);

  for (j = 0; j < n; j++) {
    struct su_register *ser;
    su_root_magic_t *magic;

    index = (int)(intptr_t)ev[j].udata;
    if (index <= 0 || self->sup_max_index < index)
      continue;
    ser = self->sup_indices[index];

    magic = ser->ser_root ? su_root_magic(ser->ser_root) : NULL;
    ser->ser_wait->revents =
      (ser->ser_wait->events | SU_WAIT_HUP) &
      (
       ((ev[j].filter == EVFILT_READ) ? SU_WAIT_IN : 0) |
       ((ev[j].filter == EVFILT_WRITE) ? SU_WAIT_OUT : 0) |
       ((ev[j].flags & EV_EOF) ? SU_WAIT_HUP : 0)
       );
    if (ser->ser_wait->revents) {
      ser->ser_cb(magic, ser->ser_wait, ser->ser_arg);
      events++;
      if (version != self->sup_registers)
	/* Callback function used su_register()/su_deregister() */
	return events;
    }
  }

  return n;
}


/** Create a port using kqueue() (or poll()/select(), if kqueue() fails).
 */
su_port_t *su_kqueue_port_create(void)
{
  su_port_t *self = NULL;
  int kq = kqueue();

  if (kq < 0) {
#if HAVE_POLL
    return su_poll_port_create();
#else
    return su_select_port_create();
#endif
  }

  self = su_home_new(sizeof *self);
  if (!self)
    goto failed;

  if (su_home_destructor(su_port_home(self), su_kqueue_port_deinit) < 0)
    goto failed;

  self->sup_kqueue = kq, kq = -1;
  self->sup_indices = su_zalloc(su_port_home(self),
				(sizeof self->sup_indices[0]) *
				(self->sup_size_indices = 64));
  if (!self->sup_indices)
    goto failed;

  if (su_socket_port_init(self->sup_base, su_kqueue_port_vtable) < 0)
    goto failed;

  self->sup_multishot = SU_ENABLE_MULTISHOT_KQUEUE;

  return self;

 failed:
  if (kq != -1)
    close(kq);
  su_home_unref(su_port_home(self));
  return NULL;
}

int su_kqueue_clone_start(su_root_t *parent,
			su_clone_r return_clone,
			su_root_magic_t *magic,
			su_root_init_f init,
			su_root_deinit_f deinit)
{
  return su_pthreaded_port_start(su_kqueue_port_create,
				 parent, return_clone, magic, init, deinit);
}

#else

su_port_t *su_kqueue_port_create(void)
{
  return su_default_port_create();
}

int su_kqueue_clone_start(su_root_t *parent,
			su_clone_r return_clone,
			su_root_magic_t *magic,
			su_root_init_f init,
			su_root_deinit_f deinit)
{
  return su_default_clone_start(parent, return_clone, magic, init, deinit);
}

#endif  /* HAVE_KQUEUE */
