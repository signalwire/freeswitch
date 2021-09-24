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
 * @CFILE su_devpoll_port.c
 *
 * Port implementation using devpoll(7)
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Kai Vehmanen <kai.vehmanen@nokia.com>
 *
 * @date Created: Fri Jan 26 20:44:14 2007 ppessi
 * @date Original: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#define su_port_s su_devpoll_port_s

#include "su_port.h"

#if HAVE_SYS_DEVPOLL_H

#include "sofia-sip/su.h"
#include "sofia-sip/su_alloc.h"

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#include <sys/devpoll.h>

#define POLL2EPOLL_NEEDED \
  (POLLIN != EPOLLIN || POLLOUT != EPOLLOUT || POLLPRI != EPOLLPRI || \
   POLLERR != EPOLLERR || POLLHUP != EPOLLHUP)

#define POLL2EPOLL(e) (e & (POLLIN|POLLOUT|POLLPRI|POLLERR|POLLHUP))
#define EPOLL2POLL(e) (e & (POLLIN|POLLOUT|POLLPRI|POLLERR|POLLHUP))

/** Port based on /dev/poll. */

struct su_devpoll_port_s {
  su_socket_port_t sup_base[1];

  /** devpoll fd */
  int              sup_devpoll;
  unsigned         sup_multishot; /**< Multishot operation? */

  unsigned         sup_registers; /** Counter incremented by
				      su_port_register() or
				      su_port_unregister()
				   */
  int              sup_n_registrations;
  int              sup_max_index; /**< Indexes are equal or smaller than this */
  int              sup_size_indices; /**< Size of allocated index table */

  /** Structure containing registration data */
  struct su_devpoll {
    struct su_devpoll *ser_next; /* Next in free list */
    su_wakeup_f     ser_cb;
    su_wakeup_arg_t*ser_arg;
    su_root_t      *ser_root;
    int             ser_id; /** registration identifier */
    su_wait_t       ser_wait[1];
  } **sup_indices;

  /** Mapping from socket to struct su_devpoll */
  struct su_devpoll **sup_devpoll_by_socket;
  int sup_max_socket;
  size_t sup_n_devpoll_by_socket;
};

static void su_devpoll_port_decref(su_port_t *self,
				   int blocking,
				   char const *who);
static int su_devpoll_port_register(su_port_t *self,
				  su_root_t *root,
				  su_wait_t *wait,
				  su_wakeup_f callback,
				  su_wakeup_arg_t *arg,
				  int priority);
static int su_devpoll_port_unregister(su_port_t *port,
				    su_root_t *root,
				    su_wait_t *wait,
				    su_wakeup_f callback,
				    su_wakeup_arg_t *arg);
static int su_devpoll_port_deregister(su_port_t *self, int i);
static int su_devpoll_port_unregister_all(su_port_t *self, su_root_t *root);
static int su_devpoll_port_eventmask(su_port_t *self,
				   int index,
				   int socket,
				   int events);
static int su_devpoll_port_multishot(su_port_t *self, int multishot);
static int su_devpoll_port_wait_events(su_port_t *self, su_duration_t tout);
static char const *su_devpoll_port_name(su_port_t const *self);

su_port_vtable_t const su_devpoll_port_vtable[1] =
  {{
      /* su_vtable_size: */ sizeof su_devpoll_port_vtable,
      su_pthread_port_lock,
      su_pthread_port_unlock,
      su_base_port_incref,
      su_devpoll_port_decref,
      su_base_port_gsource,
      su_base_port_send,
      su_devpoll_port_register,
      su_devpoll_port_unregister,
      su_devpoll_port_deregister,
      su_devpoll_port_unregister_all,
      su_devpoll_port_eventmask,
      su_base_port_run,
      su_base_port_break,
      su_base_port_step,
      su_pthread_port_thread,
      su_base_port_add_prepoll,
      su_base_port_remove_prepoll,
      su_base_port_timers,
      su_devpoll_port_multishot,
      su_devpoll_port_wait_events,
      su_base_port_getmsgs,
      su_base_port_getmsgs_from,
      su_devpoll_port_name,
      su_base_port_start_shared,
      su_pthread_port_wait,
      su_pthread_port_execute,
      su_base_port_deferrable,
      su_base_port_max_defer,
      su_socket_port_wakeup,
      su_base_port_is_running,
    }};

static char const *su_devpoll_port_name(su_port_t const *self)
{
  return "devpoll";
}

static void su_devpoll_port_decref(su_port_t *self,
				   int blocking,
				   char const *who)
{
  (void)su_base_port_decref(self, blocking, who);
}


static void su_devpoll_port_deinit(void *arg)
{
  su_port_t *self = arg;

  SU_DEBUG_9(("%s(%p) called\n", "su_devpoll_port_deinit", (void* )self));

  su_socket_port_deinit(self->sup_base);

  close(self->sup_devpoll), self->sup_devpoll = -1;
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
int su_devpoll_port_register(su_port_t *self,
			     su_root_t *root,
			     su_wait_t *wait,
			     su_wakeup_f callback,
			     su_wakeup_arg_t *arg,
			     int priority)
{
  int i, j, n;
  struct su_devpoll *ser;
  struct su_devpoll **indices = self->sup_indices;
  struct su_devpoll **devpoll_by_socket = self->sup_devpoll_by_socket;
  su_home_t *h = su_port_home(self);
  struct pollfd pollfd[1];

  assert(su_port_own_thread(self));

  if (wait->fd < 0)
    return su_seterrno(EINVAL);

  n = self->sup_size_indices;

  if (n >= SU_WAIT_MAX)
    return su_seterrno(ENOMEM);

  ser = indices[0];

  if (!ser) {
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

  if ((size_t)wait->fd >= self->sup_n_devpoll_by_socket) {
    size_t n_devpoll_by_socket = ((size_t)wait->fd + 32) / 32 * 32;

    devpoll_by_socket = su_realloc(h, devpoll_by_socket,
				   n_devpoll_by_socket *
				   (sizeof devpoll_by_socket[0]));
    if (devpoll_by_socket == NULL)
      return -1;

    memset(&devpoll_by_socket[self->sup_n_devpoll_by_socket],
	   0,
	   (char *)&devpoll_by_socket[n_devpoll_by_socket] -
	   (char *)&devpoll_by_socket[self->sup_n_devpoll_by_socket]);

    self->sup_devpoll_by_socket = devpoll_by_socket;
    self->sup_n_devpoll_by_socket = n_devpoll_by_socket;
  }


  if (devpoll_by_socket[wait->fd])
    /* XXX - we should lift this limitation with epoll, too */
    return errno = EEXIST, -1;

  i = ser->ser_id;

  pollfd->fd = wait->fd;
  pollfd->events = wait->events & ~POLLREMOVE;
  pollfd->revents = 0;

  if (write(self->sup_devpoll, pollfd, (sizeof pollfd)) != (sizeof pollfd)) {
    return errno = EIO, -1;
  }

  indices[0] = ser->ser_next;
  devpoll_by_socket[wait->fd] = ser;

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
static int su_devpoll_port_deregister0(su_port_t *self, int i, int destroy_wait)
{
  struct su_devpoll **indices = self->sup_indices;
  struct su_devpoll *ser;
  struct pollfd pollfd[1];

  ser = self->sup_indices[i];
  if (ser == NULL || ser->ser_cb == NULL) {
    su_seterrno(ENOENT);
    return -1;
  }

  assert(ser->ser_id == i);
  assert(self->sup_devpoll_by_socket[ser->ser_wait->fd] == ser);

  pollfd->fd = ser->ser_wait->fd;
  pollfd->events = POLLREMOVE;
  pollfd->revents = 0;

  if (write(self->sup_devpoll, pollfd, sizeof pollfd) == -1) {
    SU_DEBUG_1(("su_devpoll_port(%p): POLLREMOVE %d: %s\n", (void *)self,
		ser->ser_wait->fd, su_strerror(su_errno())));
  }

  if (destroy_wait)
    su_wait_destroy(ser->ser_wait);

  memset(ser, 0, sizeof *ser);
  ser->ser_id = i;
  ser->ser_next = indices[0], indices[0] = ser;
  self->sup_devpoll_by_socket[pollfd->fd] = NULL;

  self->sup_n_registrations--;
  self->sup_registers++;

  return i;
}


/** Unregister a su_wait_t object.
 *
 *  The function su_devpoll_port_unregister() unregisters a su_wait_t object. The
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
 * @deprecated Use su_devpoll_port_deregister() instead.
 *
 * @return Nonzero index of the wait object, or -1 upon an error.
 */
int su_devpoll_port_unregister(su_port_t *self,
			     su_root_t *root,
			     su_wait_t *wait,
			     su_wakeup_f callback, /* XXX - ignored */
			     su_wakeup_arg_t *arg)
{
  int i, I;

  struct su_devpoll *ser;

  assert(self);
  assert(su_port_own_thread(self));

  I = self->sup_max_index;

  for (i = 1; i <= I; i++) {
    ser = self->sup_indices[i];

    if (ser->ser_cb &&
	arg == ser->ser_arg &&
	SU_WAIT_CMP(wait[0], ser->ser_wait[0]) == 0)
      return su_devpoll_port_deregister0(self, ser->ser_id, 0);
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
int su_devpoll_port_deregister(su_port_t *self, int i)
{
  struct su_devpoll *ser;

  if (i <= 0 || i > self->sup_max_index)
    return su_seterrno(EBADF);

  ser = self->sup_indices[i];
  if (!ser->ser_cb)
    return su_seterrno(EBADF);

  return su_devpoll_port_deregister0(self, i, 1);
}


/** @internal
 * Unregister all su_wait_t objects of given su_root_t instance.
 *
 * The function su_devpoll_port_unregister_all() unregisters all su_wait_t
 * objects associated with given root object.
 *
 * @param  self     - pointer to port object
 * @param  root     - pointer to root object
 *
 * @return Number of wait objects removed.
 */
int su_devpoll_port_unregister_all(su_port_t *self, su_root_t *root)
{
  int i, I, n;

  struct su_devpoll *ser;

  assert(self); assert(root);
  assert(su_port_own_thread(self));

  I = self->sup_max_index;

  for (i = 1, n = 0; i <= I; i++) {
    ser = self->sup_indices[i];
    if (ser->ser_root != root)
      continue;
    su_devpoll_port_deregister0(self, ser->ser_id, 0);
    n++;
  }

  return n;
}

/**Set mask for a registered event. @internal
 *
 * The function su_devpoll_port_eventmask() sets the mask describing events
 * that can signal the registered callback.
 *
 * @param port   pointer to port object
 * @param index  registration index
 * @param socket socket
 * @param events new event mask
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.  */
int su_devpoll_port_eventmask(su_port_t *self, int index, int socket, int events)
{
  struct su_devpoll *ser;
  struct pollfd w[2];

  if (index <= 0 || index > self->sup_max_index)
    return su_seterrno(EBADF);

  ser = self->sup_indices[index];
  if (!ser->ser_cb)
    return su_seterrno(EBADF);

  ser->ser_wait->events = events;

  w[0].fd = socket;
  w[0].events = POLLREMOVE;
  w[0].revents = 0;
  w[1].fd = socket;
  w[1].events = events & ~POLLREMOVE;
  w[1].revents = 0;

  if (write(self->sup_devpoll, w, (sizeof w)) == -1) {
    SU_DEBUG_1(("su_devpoll_port_eventmask(%p): %d: %s\n", (void *)self,
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
int su_devpoll_port_multishot(su_port_t *self, int multishot)
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
int su_devpoll_port_wait_events(su_port_t *self, su_duration_t tout)
{
  int j, n, events = 0;
  unsigned version = self->sup_registers;

  int const M = 4;
  struct pollfd ev[M];
  struct dvpoll dp[1];

  dp->dp_fds = memset(ev, 0, sizeof ev);
  dp->dp_nfds = self->sup_multishot ? M : 1;
  dp->dp_timeout = tout >= INT_MAX ? INT_MAX : tout;

  n = ioctl(self->sup_devpoll, DP_POLL, dp);

  for (j = 0; j < n; j++) {
    int socket;
    struct su_devpoll *ser;
    su_root_magic_t *magic;

    socket = ev[j].fd;
    if (socket < 0 || self->sup_n_devpoll_by_socket <= socket)
      continue;
    ser = self->sup_devpoll_by_socket[socket]; assert(ser);

    magic = ser->ser_root ? su_root_magic(ser->ser_root) : NULL;
    ser->ser_wait->revents = ev[j].revents;
    ser->ser_cb(magic, ser->ser_wait, ser->ser_arg);
    events++;
    if (version != self->sup_registers)
      /* Callback function used su_register()/su_deregister() */
      break;
  }

  return events;
}

/** Create a port using /dev/poll or poll().
 */
su_port_t *su_devpoll_port_create(void)
{
  su_port_t *self;
  int devpoll = open("/dev/poll", O_RDWR);

  if (devpoll == -1) {
    /* Fallback to poll() */
    SU_DEBUG_3(("%s(): open(\"%s\") => %u: %s\n",
		"su_devpoll_port_create", "/dev/poll",
		errno, strerror(errno)));
    return su_poll_port_create();
  }

  self = su_home_new(sizeof *self);
  if (!self) {
    close(devpoll);
    return self;
  }

  if (su_home_destructor(su_port_home(self), su_devpoll_port_deinit) < 0 ||
      !(self->sup_indices =
	su_zalloc(su_port_home(self),
		  (sizeof self->sup_indices[0]) *
		  (self->sup_size_indices = 64)))) {
    su_home_unref(su_port_home(self));
    close(devpoll);
    return NULL;
  }

  self->sup_devpoll = devpoll;
  self->sup_multishot = SU_ENABLE_MULTISHOT_POLL;

  if (su_socket_port_init(self->sup_base, su_devpoll_port_vtable) < 0)
    return su_home_unref(su_port_home(self)), NULL;

  SU_DEBUG_9(("%s(%p): devpoll_create() => %u: %s\n",
	      "su_port_create", (void *)self, self->sup_devpoll, "OK"));

  return self;
}

int su_devpoll_clone_start(su_root_t *parent,
			 su_clone_r return_clone,
			 su_root_magic_t *magic,
			 su_root_init_f init,
			 su_root_deinit_f deinit)
{
  return su_pthreaded_port_start(su_devpoll_port_create,
				 parent, return_clone, magic, init, deinit);
}

#else

su_port_t *su_devpoll_port_create(void)
{
  return su_default_port_create();
}

int su_devpoll_clone_start(su_root_t *parent,
			 su_clone_r return_clone,
			 su_root_magic_t *magic,
			 su_root_init_f init,
			 su_root_deinit_f deinit)
{
  return su_default_clone_start(parent, return_clone, magic, init, deinit);
}

#endif  /* HAVE_EPOLL */
