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
 *
 * @CFILE su_wait.c
 * Implementation of OS-independent socket synchronization interface.
 *
 * This looks like nth reincarnation of "reactor".  It implements the
 * (poll()/select()/WaitForMultipleObjects()) functionality.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 * @date Created: Tue Sep 14 15:51:04 1999 ppessi
 *
 */

#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#define SU_INTERNAL_P su_root_t *

#include "sofia-sip/su.h"
#include "sofia-sip/su_wait.h"
#include "sofia-sip/su_alloc.h"

/**@defgroup su_wait Syncronization and Threading
 * @brief Syncronization and threading interface.
 *
 * The Sofia utility library provides simple OS-independent synchronization
 * interface. The synchronization interface contains primitives for managing
 * events, messages, timers and threads.
 *
 */

/**@ingroup su_wait
 * @defgroup su_root_ex Example and test code for syncronization and threads
 *
 * Example programs demonstrate the su syncronization and threading
 * primitives.
 */


/**@ingroup su_wait
 *
 * @page su_wait_t Wait objects
 *
 *   Wait objects are used to signal I/O events to the process.
 *   The events are as follows:
 *
 *   - SU_WAIT_IN       - incoming data is available on socket
 *   - SU_WAIT_OUT      - data can be sent on socket
 *   - SU_WAIT_ERR      - an error occurred on socket
 *   - SU_WAIT_HUP      - the socket connection was closed
 *   - SU_WAIT_ACCEPT   - a listening socket accepted a new connection attempt
 *
 *   It is possible to combine several events with |, binary or operator.
 *
 *   The wait objects can be managed with functions as follows:
 *   - su_wait_create()
 *   - su_wait_destroy()
 *   - su_wait()
 *   - su_wait_events()
 *   - su_wait_mask()
 *
 * @note
 *   In Unix, the wait object is @c struct @c poll. The structure contains a
 *   file descriptor, a mask describing expected events, and a mask
 *   containing the occurred events after calling @c su_wait(), ie. poll().
 *
 * @note
 *   In Windows, the wait object is a @c HANDLE (a descriptor of a Windows
 *   kernel entity).
 *
 */

/**Initialize a wait object.
 *
 * The function su_wait_init initializes a memory area of a su_wait_t
 * object.
 */
void su_wait_init(su_wait_t dst[1])
{
  su_wait_t const src = SU_WAIT_INIT;
  *dst = src;
}

/**Create a wait object.
 *
 * The function su_wait_create() creates a new su_wait_t object for an @a
 * socket, with given @a events.  The new wait object is assigned to the @a
 * newwait parameter.
 *
 * There can be only one wait object per socket. (This is a limitation or
 * feature of WinSock interface; the limitation is not enforced on other
 * platforms).
 *
 * As a side-effect the socket is put into non-blocking mode when wait
 * object is created.
 *
 * @param newwait  the newly created wait object (output)
 * @param socket   socket
 * @param events   mask for events that can signal this wait object
 *
 * @retval 0 if the call was successful,
 * @retval -1 upon an error.
*/
int su_wait_create(su_wait_t *newwait, su_socket_t socket, int events)
{
#if SU_HAVE_WINSOCK
  HANDLE h = WSACreateEvent();

  if (newwait == NULL || events == 0 || socket == INVALID_SOCKET) {
    su_seterrno(WSAEINVAL);
    return -1;
  }

  *newwait = 0;

  if (WSAEventSelect(socket, h, events) != 0) {
    int error = su_errno();
    WSACloseEvent(h);
    su_seterrno(error);
    return -1;
  }

  *newwait = h;

#elif SU_HAVE_POLL || HAVE_SELECT
  int mode;

  if (newwait == NULL || events == 0 || socket == INVALID_SOCKET) {
    su_seterrno(EINVAL);
    return -1;
  }

  mode = fcntl(socket, F_GETFL, 0);
  if (mode < 0)
     return -1;
  mode |= O_NDELAY | O_NONBLOCK;
  if (fcntl(socket, F_SETFL, mode) < 0)
    return -1;

  newwait->fd = socket;
  newwait->events = events;
  newwait->revents = 0;
#endif

  return 0;
}

/** Destroy a wait object.
 *
 * The function su_wait_destroy() destroys a su_wait_t object.
 *
 * @param waitobj  pointer to wait object
 *
 * @retval 0 when successful,
 * @retval -1 upon an error.
 */
int su_wait_destroy(su_wait_t *waitobj)
{
#if SU_HAVE_WINSOCK
  su_wait_t w0 = NULL;
  assert(waitobj != NULL);
  if (*waitobj) {
    WSACloseEvent(*waitobj);
    *waitobj = w0;
  }
#else
  su_wait_t w0 = { INVALID_SOCKET, 0, 0 };
  assert(waitobj != NULL);
  if (waitobj) {
    *waitobj = w0;
  }
#endif
  return waitobj ? 0 : -1;
}

/**Wait for multiple events.
 *
 * The function su_wait() blocks until an event specified by wait objects in
 * @a wait array.  If @a timeout is not SU_WAIT_FOREVER, a timeout occurs
 * after @a timeout milliseconds.
 *
 * In Unix, this is @c poll() or @c select().
 *
 * In Windows, this is @c WSAWaitForMultipleEvents().
 *
 * @param waits    array of wait objects
 * @param n        number of wait objects in array waits
 * @param timeout  timeout in milliseconds
 *
 * @retval Index of the signaled wait object, if any,
 * @retval SU_WAIT_TIMEOUT if timeout occurred, or
 * @retval -1 upon an error.
 */
int su_wait(su_wait_t waits[], unsigned n, su_duration_t timeout)
{
#if SU_HAVE_WINSOCK
  DWORD i;

  if (n > 0)
    i = WSAWaitForMultipleEvents(n, waits, FALSE, timeout, FALSE);
  else
    return Sleep(timeout), SU_WAIT_TIMEOUT;

  if (i == WSA_WAIT_TIMEOUT)
    return SU_WAIT_TIMEOUT;
  else if (i == WSA_WAIT_FAILED)
    return SOCKET_ERROR;
  else
    return i;

#elif SU_HAVE_POLL || HAVE_SELECT
  for (;;) {
    int i = poll(waits, n, timeout);

    if (i == 0)
      return SU_WAIT_TIMEOUT;

    if (i > 0) {
      unsigned j;
      for (j = 0; j < n; j++) {
	if (waits[j].revents)
	  return j;
      }
    }

    if (errno == EINTR)
      continue;

    return -1;
  }
#endif
}

/** Get events.
 *
 *   The function su_wait_events() returns an mask describing events occurred.
 *
 * @param waitobj  pointer to wait object
 * @param s        socket
 *
 * @return Binary mask describing the events.
 */
int su_wait_events(su_wait_t *waitobj, su_socket_t s)
{
#if SU_HAVE_WINSOCK
  WSANETWORKEVENTS net_events;

  if (WSAEnumNetworkEvents(s, *waitobj, &net_events) != 0)
    return SOCKET_ERROR;

  return net_events.lNetworkEvents;

#elif SU_HAVE_POLL || HAVE_SELECT
  /* poll(e, 1, 0); */
  return waitobj->revents;
#endif
}

/** Set event mask.
 *
 *   The function su_wait_mask() sets the mask describing events that can
 *   signal the wait object.
 *
 * @param waitobj  pointer to wait object
 * @param s        socket
 * @param events   new event mask
 *
 * @retval  0 when successful,
 * @retval -1 upon an error.
 */
int su_wait_mask(su_wait_t *waitobj, su_socket_t s, int events)
{
#if SU_HAVE_WINSOCK
  HANDLE e = *waitobj;

  if (WSAEventSelect(s, e, events) != 0) {
    int error = WSAGetLastError();
    WSACloseEvent(e);
    WSASetLastError(error);
    return -1;
  }

#elif SU_HAVE_POLL || HAVE_SELECT
  waitobj->fd = s;
  waitobj->events = events;
  waitobj->revents = 0;
#endif

  return 0;
}

