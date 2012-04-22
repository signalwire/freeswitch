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
 * @CFILE su_osx_runloop.c
 *
 * OS-Independent Socket Syncronization Interface.
 *
 * This looks like nth reincarnation of "reactor".  It implements the
 * poll/select/WaitForMultipleObjects and message passing functionality.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <martti.mela@nokia.com>
 *
 * @date Created: Tue Sep 14 15:51:04 1999 ppessi
 */

#include "config.h"

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define su_port_s su_osx_port_s

#include "su_port.h"
#include "sofia-sip/su_osx_runloop.h"
#include "sofia-sip/su_alloc.h"
#include "sofia-sip/su_debug.h"

#if HAVE_FUNC
#define enter (void)SU_DEBUG_9(("%s: entering\n", __func__))
#elif HAVE_FUNCTION
#define enter (void)SU_DEBUG_9(("%s: entering\n", __FUNCTION__))
#else
#define enter (void)0
#endif

static su_port_t *su_osx_runloop_create(void) __attribute__((__malloc__));

/* Callback for CFObserver and CFSocket */
static void cf_observer_cb(CFRunLoopObserverRef observer,
			   CFRunLoopActivity activity,
			   void *info);

static void su_osx_port_socket_cb(CFSocketRef s,
				  CFSocketCallBackType callbackType,
				  CFDataRef address,
				  const void *data,
				  void *info);

static void su_osx_port_deinit(void *arg);

static void su_osx_port_decref(su_port_t *self, int blocking, char const *who)
{
  (void)su_base_port_decref(self, blocking, who);
}


static CFSocketCallBackType map_poll_event_to_cf_event(int events);

static int su_osx_port_send(su_port_t *self, su_msg_r rmsg);

static int su_osx_port_register(su_port_t *self,
			    su_root_t *root,
			    su_wait_t *wait,
			    su_wakeup_f callback,
			    su_wakeup_arg_t *arg,
			    int priority);
static int su_osx_port_unregister(su_port_t *port,
			      su_root_t *root,
			      su_wait_t *wait,
			      su_wakeup_f callback,
			      su_wakeup_arg_t *arg);

static int su_osx_port_deregister(su_port_t *self, int i);

static int su_osx_port_unregister_all(su_port_t *self,
			   su_root_t *root);

static int su_osx_port_eventmask(su_port_t *, int , int, int );
static void su_osx_port_run(su_port_t *self);
static void su_osx_port_break(su_port_t *self);
static su_duration_t su_osx_port_step(su_port_t *self, su_duration_t tout);

static int su_osx_port_multishot(su_port_t *port, int multishot);

static int su_osx_port_wait_events(su_port_t *self, su_duration_t tout);

static char const *su_osx_port_name(su_port_t const *self)
{
  return "CFRunLoop";
}

/*
 * Port is a per-thread reactor.
 *
 * Multiple root objects executed by single thread share a su_port_t object.
 */
struct su_osx_port_s {
  su_socket_port_t sup_socket[1];

#define sup_pthread sup_socket->sup_base
#define sup_base sup_socket->sup_base->sup_base
#define sup_home sup_socket->sup_base->sup_base->sup_home

  unsigned         sup_source_fired;

  CFRunLoopRef        sup_main_loop;
  CFRunLoopSourceRef *sup_sources;
  CFSocketRef        *sup_sockets;

  CFRunLoopObserverRef sup_observer;
  CFRunLoopObserverContext sup_observer_cntx[1];
  /* Struct for CFSocket callbacks; contains current CFSource index */
  struct osx_magic {
    su_port_t *o_port;
    int        o_current;
    int        o_count;
  } osx_magic[1];

  unsigned         sup_multishot; /**< Multishot operation? */

  unsigned         sup_registers; /** Counter incremented by
				      su_port_register() or
				      su_port_unregister()
				   */
  int              sup_n_waits; /**< Active su_wait_t in su_waits */
  int              sup_size_waits; /**< Size of allocate su_waits */

  int              sup_pri_offset; /**< Offset to prioritized waits */

#define INDEX_MAX (0x7fffffff)

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


su_port_vtable_t const su_osx_port_vtable[1] =
  {{
      /* su_vtable_size: */ sizeof su_osx_port_vtable,
      su_pthread_port_lock,
      su_pthread_port_unlock,
      su_base_port_incref,
      su_osx_port_decref,
      su_base_port_gsource,
      su_osx_port_send,
      su_osx_port_register,
      su_osx_port_unregister,
      su_osx_port_deregister,
      su_osx_port_unregister_all,
      su_osx_port_eventmask,
      su_osx_port_run,
      su_osx_port_break,
      su_osx_port_step,
      su_pthread_port_thread,
      su_base_port_add_prepoll,
      su_base_port_remove_prepoll,
      su_base_port_timers,
      su_osx_port_multishot,
      su_osx_port_wait_events,
      su_base_port_getmsgs,
      su_base_port_getmsgs_from,
      su_osx_port_name,
      su_base_port_start_shared,
      su_pthread_port_wait,
      su_pthread_port_execute,
      su_base_port_deferrable,
      su_base_port_max_defer,
      su_socket_port_wakeup,
      su_base_port_is_running,
    }};

/* XXX - mela static void su_osx_port_destroy(su_port_t *self); */

/** Create a reactor object.
 *
 * Allocate and initialize the instance of su_root_t.
 *
 * @param magic     pointer to user data
 *
 * @return A pointer to allocated su_root_t instance, NULL on error.
 *
 * @NEW_1_12_4.
 */
su_root_t *su_root_osx_runloop_create(su_root_magic_t *magic)
{
  return su_root_create_with_port(magic, su_osx_runloop_create());
}

void osx_enabler_cb(CFSocketRef s,
		    CFSocketCallBackType type,
		    CFDataRef address,
		    const void *data,
		    void *info)
{
  CFRunLoopRef  rl;
  struct osx_magic  *magic = (struct osx_magic *) info;
  su_port_t    *self = magic->o_port;
  su_duration_t tout = 0;
  su_time_t     now = su_now();

  rl = CFRunLoopGetCurrent();

  if (self->sup_base->sup_running) {

    if (self->sup_base->sup_prepoll)
      self->sup_base->sup_prepoll(self->sup_base->sup_pp_magic, self->sup_base->sup_pp_root);

    if (self->sup_base->sup_head)
      su_base_port_getmsgs(self);

    if (self->sup_base->sup_timers)
      su_timer_expire(&self->sup_base->sup_timers, &tout, now);
  }

  CFRunLoopWakeUp(rl);
}


/**@internal
 *
 * Allocates and initializes a message port.
 *
 * @return
 *   If successful a pointer to the new message port is returned, otherwise
 *   NULL is returned.
 */
su_port_t *su_osx_runloop_create(void)
{
  su_port_t *self = su_home_new(sizeof *self);

  if (!self)
    return self;

  enter;

  if (su_home_destructor(su_port_home(self), su_osx_port_deinit) < 0)
    return su_home_unref(su_port_home(self)), NULL;

  self->sup_multishot = SU_ENABLE_MULTISHOT_POLL;

  if (su_socket_port_init(self->sup_base, su_osx_port_vtable) == 0) {
    self->osx_magic->o_port = self;
    self->sup_observer_cntx->info = self->osx_magic;
    self->sup_observer =
      CFRunLoopObserverCreate(NULL,
			      kCFRunLoopAfterWaiting | kCFRunLoopBeforeWaiting,
			      TRUE, 0, cf_observer_cb, self->sup_observer_cntx);
#if 0
    CFRunLoopAddObserver(CFRunLoopGetCurrent(),
			 self->sup_observer,
			 kCFRunLoopDefaultMode);
#endif
  }
  else
    return su_home_unref(su_port_home(self)), NULL;

  return self;
}

static
void cf_observer_cb(CFRunLoopObserverRef observer,
		    CFRunLoopActivity activity,
		    void *info)
{
  CFRunLoopRef  rl;
  struct osx_magic  *magic = (struct osx_magic *) info;
  su_port_t    *self = magic->o_port;
  su_duration_t tout = 0;
  su_time_t     now = su_now();

  rl = CFRunLoopGetCurrent();

  if (self->sup_base->sup_running) {

    if (self->sup_base->sup_prepoll)
      self->sup_base->sup_prepoll(self->sup_base->sup_pp_magic, self->sup_base->sup_pp_root);

    if (self->sup_base->sup_head)
      su_port_getmsgs(self);

    if (self->sup_base->sup_timers)
      su_timer_expire(&self->sup_base->sup_timers, &tout, now);
  } else
    SU_DEBUG_9(("cf_observer_cb(): PORT IS NOT RUNNING!\n"));

  CFRunLoopWakeUp(rl);

  return;
}

/** @internal Destroy a port. */
static void su_osx_port_deinit(void *arg)
{
  su_port_t *self = arg;

  SU_DEBUG_9(("%s(%p) called\n", "su_osx_port_deinit", (void *)self));

  su_socket_port_deinit(self->sup_base);
}

static
CFSocketCallBackType map_poll_event_to_cf_event(int events)
{
  CFSocketCallBackType type = 0;

  if (events & SU_WAIT_IN)
    type |= kCFSocketReadCallBack;

  if (events & SU_WAIT_OUT)
    type |= kCFSocketWriteCallBack;

#if 0
  if (events & SU_WAIT_CONNECT)
    type |= kCFSocketConnectCallBack;

  if (events & SU_WAIT_ACCEPT)
    type |= kCFSocketAcceptCallBack;
#endif

  return type;
}


#if 0
static
int map_cf_event_to_poll_event(CFSocketCallBackType type)
{
  int event = 0;

  if (type & kCFSocketReadCallBack)
    event |= SU_WAIT_IN;

  if (type & kCFSocketWriteCallBack)
    event |= SU_WAIT_OUT;

  if (type & kCFSocketConnectCallBack)
    event |= SU_WAIT_CONNECT;

  if (type & kCFSocketAcceptCallBack)
    event |= SU_WAIT_ACCEPT;

  return event;
}
#endif

static
void su_osx_port_socket_cb(CFSocketRef s,
			   CFSocketCallBackType type,
			   CFDataRef address,
			   const void *data,
			   void *info)
{
  struct osx_magic *magic = (struct osx_magic *) info;
  su_port_t        *self = magic->o_port;
  int               curr = magic->o_current;
  su_duration_t tout = 0;

#if SU_HAVE_POLL
  {
    su_root_t *root;
    su_wait_t *waits = self->sup_waits;
    int n = self->sup_indices[curr];

    assert(self->sup_reverses[n] == curr);

    SU_DEBUG_9(("socket_cb(%p): count %u index %d\n", self->sup_sources[n], magic->o_count, curr));

    waits[n].revents = map_poll_event_to_cf_event(type);

    root = self->sup_wait_roots[n];
    self->sup_wait_cbs[n](root ? su_root_magic(root) : NULL,
			  &waits[n],
			  self->sup_wait_args[n]);

    if (self->sup_base->sup_running) {
      su_port_getmsgs(self);

      if (self->sup_base->sup_timers)
	su_timer_expire(&self->sup_base->sup_timers, &tout, su_now());

      if (self->sup_base->sup_head)
	tout = 0;

      /* CFRunLoopWakeUp(CFRunLoopGetCurrent()); */
    }

    /* Tell to run loop an su socket fired */
    self->sup_source_fired = 1;
  }
#endif

}

/** @internal Send a message to the port. */
int su_osx_port_send(su_port_t *self, su_msg_r rmsg)
{
  CFRunLoopRef rl;

  if (self) {
    int wakeup;

    //XXX - mela SU_OSX_PORT_LOCK(self, "su_osx_port_send");

    wakeup = self->sup_base->sup_head == NULL;

    *self->sup_base->sup_tail = rmsg[0]; rmsg[0] = NULL;
    self->sup_base->sup_tail = &(*self->sup_base->sup_tail)->sum_next;

#if SU_HAVE_MBOX
    /* if (!pthread_equal(pthread_self(), self->sup_tid)) */
    if (wakeup)
    {
      assert(self->sup_mbox[MBOX_SEND] != INVALID_SOCKET);

      if (send(self->sup_mbox[MBOX_SEND], "X", 1, 0) == -1) {
#if HAVE_SOCKETPAIR
	if (su_errno() != EWOULDBLOCK)
#endif
	  su_perror("su_msg_send: send()");
      }
    }
#endif

    //XXX - mela SU_OSX_PORT_UNLOCK(self, "su_osx_port_send");

    rl = CFRunLoopGetCurrent();
    CFRunLoopWakeUp(rl);

    return 0;
  }
  else {
    su_msg_destroy(rmsg);
    return -1;
  }
}
static int o_count;

/** @internal
 *
 *  Register a @c su_wait_t object. The wait object, a callback function and
 *  a argument pointer is stored in the port object.  The callback function
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
 *   The function @su_osx_port_register returns nonzero index of the wait object,
 *   or -1 upon an error.  */
int su_osx_port_register(su_port_t *self,
			 su_root_t *root,
			 su_wait_t *wait,
			 su_wakeup_f callback,
			 su_wakeup_arg_t *arg,
			 int priority)
{
  int i, j, n;
  CFRunLoopRef rl;
  CFRunLoopSourceRef *sources, source;
  CFSocketRef cf_socket, *sockets;
  int events = 0;
  struct osx_magic *osx_magic = NULL;
  CFSocketContext cf_socket_cntx[1] = {{0, NULL, NULL, NULL, NULL}};
  CFOptionFlags flags = 0;

  // XXX - mela assert(SU_OSX_PORT_OWN_THREAD(self));

  n = self->sup_n_waits;

  if (n >= SU_WAIT_MAX)
    return su_seterrno(ENOMEM);

  if (n >= self->sup_size_waits) {
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

    indices = realloc(self->sup_indices, (size + 1) * sizeof(*indices));
    if (indices) {
      self->sup_indices = indices;

      for (i = self->sup_size_waits; i <= size; i++)
	indices[i] = -1 - i;
    }

    reverses = realloc(self->sup_reverses, size * sizeof(*waits));
    if (reverses) {
      for (i = self->sup_size_waits; i < size; i++)
	reverses[i] = -1;
      self->sup_reverses = reverses;
    }

    sources = realloc(self->sup_sources, size * sizeof(*sources));
    if (sources)
      self->sup_sources = sources;

    sockets = realloc(self->sup_sockets, size * sizeof(*sockets));
    if (sockets)
      self->sup_sockets = sockets;

    waits = realloc(self->sup_waits, size * sizeof(*waits));
    if (waits)
      self->sup_waits = waits;

    wait_cbs = realloc(self->sup_wait_cbs, size * sizeof(*wait_cbs));
    if (wait_cbs)
      self->sup_wait_cbs = wait_cbs;

    wait_args = realloc(self->sup_wait_args, size * sizeof(*wait_args));
    if (wait_args)
      self->sup_wait_args = wait_args;

    /* Add sup_wait_roots array, if needed */
    wait_tasks = realloc(self->sup_wait_roots, size * sizeof(*wait_tasks));
    if (wait_tasks)
      self->sup_wait_roots = wait_tasks;

    if (!(indices &&
	  reverses && sources && sockets && waits && wait_cbs && wait_args && wait_tasks)) {
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
      self->sup_reverses[n] = self->sup_reverses[n-1];
      self->sup_sources[n] = self->sup_sources[n-1];
      self->sup_sockets[n] = self->sup_sockets[n-1];
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

  /* XXX -- mela: leak, leak -- free() somewheeeere */
  osx_magic = calloc(1, sizeof(*osx_magic));
  osx_magic->o_port = self;
  osx_magic->o_current = i;
  osx_magic->o_count = ++o_count;
  cf_socket_cntx->info = osx_magic;

  events = map_poll_event_to_cf_event(wait->events);

  cf_socket = CFSocketCreateWithNative(NULL,
				       (CFSocketNativeHandle) su_wait_socket(wait),
				       events, su_osx_port_socket_cb, cf_socket_cntx);

  flags = CFSocketGetSocketFlags(cf_socket);
  flags &= ~kCFSocketCloseOnInvalidate;

  CFSocketSetSocketFlags(cf_socket, flags);

  CFRetain(cf_socket);
  source = CFSocketCreateRunLoopSource(NULL, cf_socket, 0);

  SU_DEBUG_9(("source(%p): count %u index %d\n", source, o_count, i));

  rl = CFRunLoopGetCurrent();

  CFRunLoopAddSource(rl, source, kCFRunLoopDefaultMode);

  CFRetain(source);
  self->sup_sources[n] = source;
  self->sup_sockets[n] = cf_socket;

  CFRunLoopWakeUp(rl);

  /* Just like epoll, we return -1 or positive integer */

  return i;
}

/** Deregister a su_wait_t object. */
static
int su_osx_port_deregister0(su_port_t *self, int i)
{
  CFRunLoopRef rl;
  int n, N, *indices, *reverses;

  indices = self->sup_indices;
  reverses = self->sup_reverses;

  n = indices[i]; assert(n >= 0); assert(i == reverses[n]);

  N = --self->sup_n_waits;

  rl = CFRunLoopGetCurrent();
  CFSocketInvalidate(self->sup_sockets[n]);
  CFRelease(self->sup_sockets[n]);
  CFRunLoopRemoveSource(rl, self->sup_sources[n], kCFRunLoopDefaultMode);
  CFRelease(self->sup_sources[n]);

  CFRunLoopWakeUp(rl);

  if (n < self->sup_pri_offset) {
    int j = --self->sup_pri_offset;
    if (n != j) {
      assert(reverses[j] > 0);
      assert(indices[reverses[j]] == j);
      indices[reverses[j]] = n;
      reverses[n] = reverses[j];

      self->sup_sources[n] = self->sup_sources[j];
      self->sup_sockets[n] = self->sup_sockets[j];
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

    self->sup_sources[n] = self->sup_sources[N];
    self->sup_sockets[n] = self->sup_sockets[N];
    self->sup_waits[n] = self->sup_waits[N];
    self->sup_wait_cbs[n] = self->sup_wait_cbs[N];
    self->sup_wait_args[n] = self->sup_wait_args[N];
    self->sup_wait_roots[n] = self->sup_wait_roots[N];
    n = N;
  }


  reverses[n] = -1;
  memset(&self->sup_waits[n], 0, sizeof self->sup_waits[n]);
  self->sup_sources[n] = NULL;
  self->sup_sockets[n] = NULL;
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
 *  The function su_osx_port_unregister() unregisters a su_wait_t object. The
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
 * @return Nonzero index of the wait object, or -1 upon an error.
 */
int su_osx_port_unregister(su_port_t *self,
		       su_root_t *root,
		       su_wait_t *wait,
		       su_wakeup_f callback, /* XXX - ignored */
		       su_wakeup_arg_t *arg)
{
  int n, N;

  assert(self);
  // XXX - mela assert(SU_OSX_PORT_OWN_THREAD(self));

  N = self->sup_n_waits;

  for (n = 0; n < N; n++) {
    if (SU_WAIT_CMP(wait[0], self->sup_waits[n]) == 0) {
      return su_osx_port_deregister0(self, self->sup_reverses[n]);
    }
  }

  su_seterrno(ENOENT);

  return -1;
}

/** Deregister a su_wait_t object.
 *
 *  The function su_osx_port_deregister() deregisters a su_wait_t registrattion.
 *  The wait object, a callback function and a argument are removed from the
 *  port object.
 *
 * @param self     - pointer to port object
 * @param i        - registration index
 *
 * @return Index of the wait object, or -1 upon an error.
 */
int su_osx_port_deregister(su_port_t *self, int i)
{
  su_wait_t wait[1] = { SU_WAIT_INIT };
  int retval;

  assert(self);
  // XXX - mela assert(SU_OSX_PORT_OWN_THREAD(self));

  if (i <= 0 || i > self->sup_size_waits)
    return su_seterrno(EBADF);

  if (self->sup_indices[i] < 0)
    return su_seterrno(EBADF);

  retval = su_osx_port_deregister0(self, i);

  su_wait_destroy(wait);

  return retval;
}


/** @internal
 * Unregister all su_wait_t objects.
 *
 * The function su_osx_port_unregister_all() unregisters all su_wait_t objects
 * and destroys all queued timers associated with given root object.
 *
 * @param  self     - pointer to port object
 * @param  root     - pointer to root object
 *
 * @return Number of wait objects removed.
 */
int su_osx_port_unregister_all(su_port_t *self,
			   su_root_t *root)
{
  int i, j, index, N;
  int                *indices, *reverses;
  su_wait_t          *waits;
  su_wakeup_f        *wait_cbs;
  su_wakeup_arg_t   **wait_args;
  su_root_t         **wait_roots;
  CFRunLoopRef        rl;
  CFRunLoopSourceRef *sources;
  CFSocketRef        *sockets;

  // XXX - assert(SU_OSX_PORT_OWN_THREAD(self));

  N          = self->sup_n_waits;
  indices    = self->sup_indices;
  reverses   = self->sup_reverses;
  sources    = self->sup_sources;
  sockets    = self->sup_sockets;
  waits      = self->sup_waits;
  wait_cbs   = self->sup_wait_cbs;
  wait_args  = self->sup_wait_args;
  wait_roots = self->sup_wait_roots;

  rl = CFRunLoopGetCurrent();

  for (i = j = 0; i < N; i++) {
    index = reverses[i]; assert(index > 0 && indices[index] == i);

    if (wait_roots[i] == root) {
      if (i < self->sup_pri_offset)
	self->sup_pri_offset--;

      indices[index] = indices[0];
      indices[0] = -index;
      continue;
    }

    if (i != j) {
      indices[index] = j;

      CFSocketInvalidate(self->sup_sockets[j]);
      CFRelease(self->sup_sockets[j]);
      CFRunLoopRemoveSource(rl, sources[j], kCFRunLoopDefaultMode);
      CFRelease(sources[j]);

      reverses[j]   = reverses[i];
      sources[j]    = sources[i];
      sockets[j]    = sockets[i];
      waits[j]      = waits[i];
      wait_cbs[j]   = wait_cbs[i];
      wait_args[j]  = wait_args[i];
      wait_roots[j] = wait_roots[i];
    }

    j++;
  }

  /* Prepare for removing CFSources */
  for (i = j; i < N; i++) {
    reverses[i] = -1;

    CFSocketInvalidate(self->sup_sockets[i]);
    CFRelease(self->sup_sockets[i]);
    CFRunLoopRemoveSource(rl, sources[i], kCFRunLoopDefaultMode);
    CFRunLoopSourceInvalidate(sources[i]);

    sources[i] = NULL;
    sockets[i] = NULL;
    wait_cbs[i] = NULL;
    wait_args[i] = NULL;
    wait_roots[i] = NULL;
  }
  memset(&waits[j], 0, (char *)&waits[N] - (char *)&waits[j]);

  /* Tell run loop things have changed */
  CFRunLoopWakeUp(rl);

  self->sup_n_waits = j;
  self->sup_registers++;

  return N - j;
}

/**Set mask for a registered event. @internal
 *
 * The function su_osx_port_eventmask() sets the mask describing events that can
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
int su_osx_port_eventmask(su_port_t *self, int index, int socket, int events)
{
  int n, ret;

  assert(self);
  // XXX - mela assert(SU_OSX_PORT_OWN_THREAD(self));

  if (index <= 0 || index > self->sup_size_waits)
    return su_seterrno(EBADF);
  n = self->sup_indices[index];
  if (n < 0)
    return su_seterrno(EBADF);

  ret = su_wait_mask(&self->sup_waits[n], socket, events);

  CFSocketSetSocketFlags(self->sup_sockets[n],
			 map_poll_event_to_cf_event(events));

  return ret;
}

/** @internal
 *
 *  Copies the su_wait_t objects from the port. The number of wait objects
 *  can be found out by calling su_osx_port_query() with @a n_waits as zero.
 *
 * @note This function is called only by friends.
 *
 * @param self     - pointer to port object
 * @param waits    - pointer to array to which wait objects are copied
 * @param n_waits  - number of wait objects fitting in array waits
 *
 * @return Number of wait objects, or 0 upon an error.
 */
unsigned su_osx_port_query(su_port_t *self, su_wait_t *waits, unsigned n_waits)
{
  unsigned n;

  // XXX - mela assert(SU_OSX_PORT_OWN_THREAD(self));

  n = self->sup_n_waits;

  if (n_waits != 0) {
    if (waits && n_waits >= n)
      memcpy(waits, self->sup_waits, n * sizeof(*waits));
    else
      n = 0;
  }

  return n;
}

/** @internal Enable multishot mode.
 *
 * The function su_osx_port_multishot() enables, disables or queries the
 * multishot mode for the port. The multishot mode determines how the events
 * are scheduled by port. If multishot mode is enabled, port serves all the
 * sockets that have received network events. If it is disables, only first
 * socket event is served.
 *
 * @param self      pointer to port object
 * @param multishot multishot mode (0 => disables, 1 => enables, -1 => query)
 *
 * @retval 0 multishot mode is disabled
 * @retval 1 multishot mode is enabled
 * @retval -1 an error occurred
 */
int su_osx_port_multishot(su_port_t *self, int multishot)
{
  if (multishot < 0)
    return self->sup_multishot;
  else if (multishot == 0 || multishot == 1)
    return self->sup_multishot = multishot;
  else
    return (errno = EINVAL), -1;
}

#if 0
/** @internal Enable threadsafe operation. */
static
int su_osx_port_threadsafe(su_port_t *port)
{
  return su_home_threadsafe(port->sup_home);
}
#endif

/** Prepare root to be run on OSX Run Loop.
 *
 * Sets #su_root_t object to be callable by the application's run loop. This
 * function is to be used instead of su_root_run() for OSX applications
 * using Core Foundation's Run Loop.
 *
 * The function su_root_osx_prepare_run() returns immmediately.
 *
 * @param root     pointer to root object
 *
 * @NEW_1_12_4.
 */
void su_root_osx_prepare_run(su_root_t *root)
{
  su_port_t *self = root->sur_task->sut_port;
  CFRunLoopRef rl;
  su_duration_t tout = 0;

  // XXX - mela assert(SU_OSX_PORT_OWN_THREAD(self));

  enter;

  self->sup_base->sup_running = 1;
  rl = CFRunLoopGetCurrent();

  if (self->sup_base->sup_prepoll)
    self->sup_base->sup_prepoll(self->sup_base->sup_pp_magic, self->sup_base->sup_pp_root);

  if (self->sup_base->sup_head)
    su_port_getmsgs(self);

  if (self->sup_base->sup_timers)
    su_timer_expire(&self->sup_base->sup_timers, &tout, su_now());

  if (!self->sup_base->sup_running)
    return;

  CFRetain(rl);
  self->sup_main_loop = rl;

  return;
}

/** @internal Main loop.
 *
 * The function @c su_osx_port_run() waits for wait objects and the timers
 * associated with the port object.  When any wait object is signaled or
 * timer is expired, it invokes the callbacks, and returns waiting.
 *
 * The function @c su_osx_port_run() runs until @c su_osx_port_break() is called
 * from a callback.
 *
 * @param self     pointer to port object
 *
 */
void su_osx_port_run(su_port_t *self)
{
  CFRunLoopRef rl;
  su_duration_t tout = 0;

  // XXX - mela assert(SU_OSX_PORT_OWN_THREAD(self));

  enter;

  self->sup_base->sup_running = 1;
  rl = CFRunLoopGetCurrent();

  if (self->sup_base->sup_prepoll)
    self->sup_base->sup_prepoll(self->sup_base->sup_pp_magic, self->sup_base->sup_pp_root);

  if (self->sup_base->sup_head)
    su_port_getmsgs(self);

  if (self->sup_base->sup_timers)
    su_timer_expire(&self->sup_base->sup_timers, &tout, su_now());

  if (!self->sup_base->sup_running)
    return;

  CFRetain(rl);
  self->sup_main_loop = rl;

  /* if there are messages do a quick wait */
  if (self->sup_base->sup_head)
    tout = 0;

  CFRunLoopRun();

  self->sup_main_loop = NULL;

}

#if tuning
/* This version can help tuning... */
void su_osx_port_run_tune(su_port_t *self)
{
  int i;
  int timers = 0, messages = 0, events = 0;
  su_duration_t tout = 0, tout0;
  su_time_t started = su_now(), woken = started, bedtime = woken;

  // XXX - mela assert(SU_OSX_PORT_OWN_THREAD(self));

  for (self->sup_base->sup_running = 1; self->sup_base->sup_running;) {
    tout0 = tout, tout = 2000;

    timers = 0, messages = 0;

    if (self->sup_base->sup_prepoll)
      self->sup_base->sup_prepoll(self->sup_base->sup_pp_magic, self->sup_base->sup_pp_root);

    if (self->sup_base->sup_head)
      messages = su_port_getmsgs(self);

    if (self->sup_base->sup_timers)
      timers = su_timer_expire(&self->sup_base->sup_timers, &tout, su_now());

    if (!self->sup_base->sup_running)
      break;

    if (self->sup_base->sup_head)      /* if there are messages do a quick wait */
      tout = 0;

    bedtime = su_now();

    events = su_osx_port_wait_events(self, tout);

    woken = su_now();

    if (messages || timers || events)
      SU_DEBUG_1(("su_osx_port_run(%p): %.6f: %u messages %u timers %u "
		  "events slept %.6f/%.3f\n",
		  self, su_time_diff(woken, started), messages, timers, events,
		  su_time_diff(woken, bedtime), tout0 * 1e-3));

    if (!self->sup_base->sup_running)
      break;
  }
}
#endif

/** @internal
 * The function @c su_osx_port_break() is used to terminate execution of @c
 * su_osx_port_run(). It can be called from a callback function.
 *
 * @param self     pointer to port
 *
 */
void su_osx_port_break(su_port_t *self)
{
  if (self->sup_main_loop)
    CFRunLoopStop(self->sup_main_loop);

  self->sup_base->sup_running = 0;
}

/** @internal
 * The function @c su_osx_port_wait_events() is used to poll() for wait objects
 *
 * @param self     pointer to port
 * @param tout     timeout in milliseconds
 *
 * @return number of events handled
 */
static
int su_osx_port_wait_events(su_port_t *self, su_duration_t tout)
{
  int i, events = 0;
  su_wait_t *waits = self->sup_waits;
  unsigned n = self->sup_n_waits;
#if HAVE_POLL
  unsigned version = self->sup_registers;
#endif
  su_root_t *root;

  i = su_wait(waits, n, tout);

  if (i >= 0 && (unsigned)i < n) {
#if HAVE_POLL
    /* poll() can return events for multiple wait objects */
    if (self->sup_multishot) {
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
#else /* !HAVE_POLL */
    if (0) {
    }
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
su_duration_t su_osx_port_step(su_port_t *self, su_duration_t tout)
{
  CFRunLoopRef rl;
  su_time_t now = su_now();
  CFAbsoluteTime start;
  int ret, timeout = tout > INT32_MAX ? INT32_MAX : tout;

  rl = CFRunLoopGetCurrent();

  if (!rl)
    return -1;

  CFRunLoopWakeUp(rl);

  if (tout < timeout)
    timeout = tout;

  if (self->sup_base->sup_prepoll)
    self->sup_base->sup_prepoll(self->sup_base->sup_pp_magic, self->sup_base->sup_pp_root);

  if (self->sup_base->sup_head)
    su_base_port_getmsgs(self);

  if (self->sup_base->sup_timers)
    su_timer_expire(&self->sup_base->sup_timers, &tout, now);

  /* if there are messages do a quick wait */
  if (self->sup_base->sup_head)
    tout = 0;

  ret = CFRunLoopRunInMode(kCFRunLoopDefaultMode,
			   tout/1000000.0,
			   true);

  CFRunLoopWakeUp(rl);

  if (self->sup_base->sup_head)
    su_base_port_getmsgs(self);

  if (self->sup_base->sup_timers)
    su_timer_expire(&self->sup_base->sup_timers, &tout, su_now());

  if (self->sup_base->sup_head)
    tout = 0;

  return tout;
}
