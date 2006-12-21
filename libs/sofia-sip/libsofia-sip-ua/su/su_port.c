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
 * @CFILE su_port.c
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

/* React to multiple events per one poll() to make sure 
 * that high-priority events can never completely mask other events.
 * Enabled by default on all platforms except WIN32 */
#ifndef WIN32
#define SU_ENABLE_MULTISHOT_POLL 1
#else
#define SU_ENABLE_MULTISHOT_POLL 0
#endif

#include <stdlib.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

#define SU_PORT_IMPLEMENTATION 1

#include "sofia-sip/su.h"
#include "su_port.h"
#include "sofia-sip/su_alloc.h"

#if SU_HAVE_PTHREADS
/* Pthread implementation */
#include <pthread.h>
#define SU_HAVE_MBOX 1
#else
#define SU_HAVE_MBOX 0
#endif

#if HAVE_SOCKETPAIR
#define MBOX_SEND  1
#else
#define MBOX_SEND  0
#endif

#if HAVE_EPOLL
#include <sys/epoll.h>

#define POLL2EPOLL_NEEDED \
  (POLLIN != EPOLLIN || POLLOUT != EPOLLOUT || POLLPRI != EPOLLPRI || \
   POLLERR != EPOLLERR || POLLHUP != EPOLLHUP)
 
#define POLL2EPOLL(e) (e & (POLLIN|POLLOUT|POLLPRI|POLLERR|POLLHUP))
#define EPOLL2POLL(e) (e & (POLLIN|POLLOUT|POLLPRI|POLLERR|POLLHUP))

#endif

static void su_port_lock(su_port_t *self, char const *who);
static void su_port_unlock(su_port_t *self, char const *who);
static void su_port_incref(su_port_t *self, char const *who);
static void su_port_decref(su_port_t *self, int blocking, char const *who);

static struct _GSource *su_port_gsource(su_port_t *port);

static int su_port_send(su_port_t *self, su_msg_r rmsg);

static int su_port_register(su_port_t *self,
			    su_root_t *root, 
			    su_wait_t *wait, 
			    su_wakeup_f callback,
			    su_wakeup_arg_t *arg,
			    int priority);
static int su_port_unregister(su_port_t *port,
			      su_root_t *root, 
			      su_wait_t *wait,	
			      su_wakeup_f callback, 
			      su_wakeup_arg_t *arg);

static int su_port_deregister(su_port_t *self, int i);

static int su_port_unregister_all(su_port_t *self,
			   su_root_t *root);
static
int su_port_eventmask(su_port_t *self, int index, int socket, int events);
static
void su_port_run(su_port_t *self);
static
void su_port_break(su_port_t *self);
static
su_duration_t su_port_step(su_port_t *self, su_duration_t tout);

static
int su_port_own_thread(su_port_t const *port);

static
int su_port_add_prepoll(su_port_t *port,
			su_root_t *root, 
			su_prepoll_f *, 
			su_prepoll_magic_t *);

static
int su_port_remove_prepoll(su_port_t *port,
			   su_root_t *root);

static
su_timer_t **su_port_timers(su_port_t *port);

static
int su_port_multishot(su_port_t *port, int multishot);

static
int su_port_threadsafe(su_port_t *port);

static
int su_port_yield(su_port_t *port);

su_port_vtable_t const su_port_vtable[1] =
  {{
      /* su_vtable_size: */ sizeof su_port_vtable,
      su_port_lock,
      su_port_unlock,
      su_port_incref,
      su_port_decref,
      su_port_gsource,
      su_port_send,
      su_port_register,
      su_port_unregister,
      su_port_deregister,
      su_port_unregister_all,
      su_port_eventmask,
      su_port_run,
      su_port_break,
      su_port_step,
      su_port_own_thread,
      su_port_add_prepoll,
      su_port_remove_prepoll,
      su_port_timers,
      su_port_multishot,
      su_port_threadsafe,
      su_port_yield
    }};

static int su_port_wait_events(su_port_t *self, su_duration_t tout);


/** 
 * Port is a per-thread reactor.  
 *
 * Multiple root objects executed by single thread share a su_port_t object. 
 */
struct su_port_s {
  su_home_t        sup_home[1];

  su_port_vtable_t const *sup_vtable;
  
  unsigned         sup_running;

#if SU_HAVE_PTHREADS
  pthread_t        sup_tid;
  pthread_mutex_t  sup_mutex[1];
#if __CYGWIN__
  pthread_mutex_t  sup_reflock[1];
  int              sup_ref;
#else
  pthread_rwlock_t sup_ref[1];
#endif
#else
  int              sup_ref;
#endif

#if SU_HAVE_MBOX
  su_socket_t      sup_mbox[MBOX_SEND + 1];
  su_wait_t        sup_mbox_wait;
#endif

#if HAVE_EPOLL
  /** epoll() fd */
  int              sup_epoll;
#endif

  unsigned         sup_multishot; /**< Multishot operation? */

  unsigned         sup_registers; /** Counter incremented by 
				      su_port_register() or 
				      su_port_unregister()
				   */
  int              sup_n_waits; /**< Active su_wait_t in su_waits */
  int              sup_size_waits; /**< Size of allocate su_waits */

  int              sup_pri_offset; /**< Offset to prioritized waits */

#if !SU_HAVE_WINSOCK
#define INDEX_MAX (0x7fffffff)
#else 
  /* We use WSAWaitForMultipleEvents() */
#define INDEX_MAX (64)
#endif

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

  /* Pre-poll callback */
  su_prepoll_f    *sup_prepoll; 
  su_prepoll_magic_t *sup_pp_magic;
  su_root_t       *sup_pp_root;

  /* Timer list */
  su_timer_t      *sup_timers;

  /* Message list - this is protected by lock  */
  su_msg_t        *sup_head;
  su_msg_t       **sup_tail;
};

#if SU_HAVE_PTHREADS
#define SU_PORT_OWN_THREAD(p)   (pthread_equal((p)->sup_tid, pthread_self()))

#if __CYGWIN__

/* Debugging versions */
#define SU_PORT_INITREF(p)      (pthread_mutex_init((p)->sup_reflock, NULL), printf("initref(%p)\n", (p)))
#define SU_PORT_INCREF(p, f)    (pthread_mutex_lock(p->sup_reflock), p->sup_ref++, pthread_mutex_unlock(p->sup_reflock), printf("incref(%p) by %s\n", (p), f))
#define SU_PORT_DECREF(p, f)    do {					\
    pthread_mutex_lock(p->sup_reflock);	p->sup_ref--; pthread_mutex_unlock(p->sup_reflock); \
    if ((p->sup_ref) == 0) {			\
      printf("decref(%p) to 0 by %s\n", (p), f); su_port_destroy(p); }	\
    else { printf("decref(%p) to %u by %s\n", (p), p->sup_ref, f); }  } while(0)

#define SU_PORT_ZAPREF(p, f)    do { printf("zapref(%p) by %s\n", (p), f), \
    pthread_mutex_lock(p->sup_reflock);	p->sup_ref--; pthread_mutex_unlock(p->sup_reflock); \
  if ((p->sup_ref) != 0) { \
    assert(!"SU_PORT_ZAPREF"); } \
  su_port_destroy(p); } while(0)

#define SU_PORT_INITLOCK(p) \
   (pthread_mutex_init((p)->sup_mutex, NULL), printf("init_lock(%p)\n", p))

#define SU_PORT_LOCK(p, f)    \
   (printf("%ld at %s locking(%p)...", pthread_self(), f, p), pthread_mutex_lock((p)->sup_mutex), printf(" ...%ld at %s locked(%p)...", pthread_self(), f, p))

#define SU_PORT_UNLOCK(p, f)  \
  (pthread_mutex_unlock((p)->sup_mutex), printf(" ...%ld at %s unlocked(%p)\n", pthread_self(), f, p))

#elif 1
#define SU_PORT_INITREF(p)      (pthread_rwlock_init(p->sup_ref, NULL))
#define SU_PORT_INCREF(p, f)    (pthread_rwlock_rdlock(p->sup_ref))
#define SU_PORT_DECREF(p, f)    do { pthread_rwlock_unlock(p->sup_ref); \
  if (pthread_rwlock_trywrlock(p->sup_ref) == 0) su_port_destroy(p); } while(0)

#define SU_PORT_ZAPREF(p, f)    do { pthread_rwlock_unlock(p->sup_ref); \
  if (pthread_rwlock_trywrlock(p->sup_ref) != 0) { \
    assert(!"SU_PORT_ZAPREF"); pthread_rwlock_wrlock(p->sup_ref); } \
  su_port_destroy(p); } while(0)

#define SU_PORT_INITLOCK(p)     (pthread_mutex_init((p)->sup_mutex, NULL))
#define SU_PORT_LOCK(p, f)      (pthread_mutex_lock((p)->sup_mutex))
#define SU_PORT_UNLOCK(p, f)    (pthread_mutex_unlock((p)->sup_mutex))

#else

/* Debugging versions */
#define SU_PORT_INITREF(p)      (pthread_rwlock_init((p)->sup_ref, NULL), printf("initref(%p)\n", (p)))
#define SU_PORT_INCREF(p, f)    (pthread_rwlock_rdlock(p->sup_ref), printf("incref(%p) by %s\n", (p), f))
#define SU_PORT_DECREF(p, f)    do {					\
    pthread_rwlock_unlock(p->sup_ref);					\
    if (pthread_rwlock_trywrlock(p->sup_ref) == 0) {			\
      printf("decref(%p) to 0 by %s\n", (p), f); su_port_destroy(p); }	\
    else { printf("decref(%p) by %s\n", (p), f); }  } while(0)

#define SU_PORT_ZAPREF(p, f)    do { printf("zapref(%p) by %s\n", (p), f), \
  pthread_rwlock_unlock(p->sup_ref); \
  if (pthread_rwlock_trywrlock(p->sup_ref) != 0) { \
    assert(!"SU_PORT_ZAPREF"); pthread_rwlock_wrlock(p->sup_ref); } \
  su_port_destroy(p); } while(0)

#define SU_PORT_INITLOCK(p) \
   (pthread_mutex_init((p)->sup_mutex, NULL), printf("init_lock(%p)\n", p))

#define SU_PORT_LOCK(p, f)    \
   (printf("%ld at %s locking(%p)...", pthread_self(), f, p), pthread_mutex_lock((p)->sup_mutex), printf(" ...%ld at %s locked(%p)...", pthread_self(), f, p))

#define SU_PORT_UNLOCK(p, f)  \
  (pthread_mutex_unlock((p)->sup_mutex), printf(" ...%ld at %s unlocked(%p)\n", pthread_self(), f, p))

#endif

#else /* !SU_HAVE_PTHREADS */

#define SU_PORT_OWN_THREAD(p)  1
#define SU_PORT_INITLOCK(p)    (void)(p)
#define SU_PORT_LOCK(p, f)      (void)(p)
#define SU_PORT_UNLOCK(p, f)    (void)(p)
#define SU_PORT_ZAPREF(p, f)    ((p)->sup_ref--)

#define SU_PORT_INITREF(p)      ((p)->sup_ref = 1)
#define SU_PORT_INCREF(p, f)    ((p)->sup_ref++)
#define SU_PORT_DECREF(p, f) \
do { if (--((p)->sup_ref) == 0) su_port_destroy(p); } while (0);

#endif

#if SU_HAVE_MBOX
static int su_port_wakeup(su_root_magic_t *magic, 
			  su_wait_t *w,
			  su_wakeup_arg_t *arg);
#endif

static void su_port_destroy(su_port_t *self);

/**@internal
 *
 * Allocates and initializes a message port. It creates a mailbox used to.
 * wake up the tasks waiting on the port if needed.  Currently, the
 * mailbox is simply an UDP socket connected to itself.
 *
 * @return
 *   If successful a pointer to the new message port is returned, otherwise
 *   NULL is returned.  
 */
su_port_t *su_port_create(void)
{
  su_port_t *self;

  SU_DEBUG_9(("su_port_create() called\n"));

  self = su_home_clone(NULL, sizeof(*self));

  if (self) {
#if SU_HAVE_MBOX
    int af;
    su_socket_t mb = INVALID_SOCKET;
    char const *why;
#endif

    self->sup_vtable = su_port_vtable;
    
    SU_PORT_INITREF(self);
    SU_PORT_INITLOCK(self);
    self->sup_tail = &self->sup_head;

    self->sup_multishot = (SU_ENABLE_MULTISHOT_POLL) != 0;

#if SU_HAVE_PTHREADS
    self->sup_tid = pthread_self();
#endif

#if HAVE_EPOLL
    self->sup_epoll = epoll_create(su_root_size_hint);
    if (self->sup_epoll == -1)
      SU_DEBUG_3(("su_port(%p): epoll_create(): %s\n", self, strerror(errno)));
    else
      SU_DEBUG_9(("su_port(%p): epoll_create() => %u: OK\n",
		  self, self->sup_epoll));
#endif
  
#if SU_HAVE_MBOX
#if HAVE_SOCKETPAIR
#if defined(AF_LOCAL)
    af = AF_LOCAL;
#else
    af = AF_UNIX;
#endif
    if (socketpair(af, SOCK_STREAM, 0, self->sup_mbox) == -1) {
      why = "su_port_init: socketpair"; goto error;
    }

    mb = self->sup_mbox[0];
    su_setblocking(self->sup_mbox[0], 0);
    su_setblocking(self->sup_mbox[1], 0);
#else
    {
      struct sockaddr_in sin = { sizeof(struct sockaddr_in), 0 };
      socklen_t sinsize = sizeof sin;
      struct sockaddr *sa = (struct sockaddr *)&sin;

      af = PF_INET;

      self->sup_mbox[0] = mb = su_socket(af, SOCK_DGRAM, IPPROTO_UDP);
      if (mb == INVALID_SOCKET) {
	why = "su_port_init: socket"; goto error;
      }
  
      sin.sin_family = AF_INET;
      sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK); /* 127.1 */
      
      /* Get a port for us */
      if (bind(mb, sa, sizeof sin) == -1) {
	why = "su_port_init: bind"; goto error;
      }

      if (getsockname(mb, sa, &sinsize) == -1) {
	why = "su_port_init: getsockname"; goto error;
      }
    
      if (connect(mb, sa, sinsize) == -1) {
	why = "su_port_init: connect"; goto error;
      }
    }
#endif    

    if (su_wait_create(&self->sup_mbox_wait, mb, SU_WAIT_IN) == -1) {
      why = "su_port_init: su_wait_create"; goto error;
    }

    if (su_port_register(self, NULL, &self->sup_mbox_wait, su_port_wakeup, 
			 (su_wakeup_arg_t *)self->sup_mbox, 0)
	== -1) {
      why = "su_port_create: su_port_register"; goto error;
    }

    SU_DEBUG_9(("su_port_create() returns %p\n", self));

    return self;

  error:
    su_perror(why);
    su_port_destroy(self), self = NULL;
#endif
  }

  SU_DEBUG_9(("su_port_create() returns %p\n", self));

  return self;
}

/** @internal Destroy a port. */
void su_port_destroy(su_port_t *self)
{
  assert(self);

  SU_DEBUG_9(("su_port_destroy() called\n"));

#if SU_HAVE_MBOX
  if (self->sup_mbox[0] != INVALID_SOCKET) {
    su_port_unregister(self, NULL, &self->sup_mbox_wait, NULL, 
		       (su_wakeup_arg_t *)self->sup_mbox);
    su_wait_destroy(&self->sup_mbox_wait);
    su_close(self->sup_mbox[0]); self->sup_mbox[0] = INVALID_SOCKET;
#if HAVE_SOCKETPAIR
    su_close(self->sup_mbox[1]); self->sup_mbox[1] = INVALID_SOCKET;
#endif
    SU_DEBUG_9(("su_port_destroy() close mailbox\n"));
  }
#endif
  if (self->sup_waits) 
    free(self->sup_waits), self->sup_waits = NULL;
  if (self->sup_wait_cbs)
    free(self->sup_wait_cbs), self->sup_wait_cbs = NULL;
  if (self->sup_wait_args)
    free(self->sup_wait_args), self->sup_wait_args = NULL;
  if (self->sup_wait_roots)
    free(self->sup_wait_roots), self->sup_wait_roots = NULL;
  if (self->sup_reverses)
    free(self->sup_reverses), self->sup_reverses = NULL;
  if (self->sup_indices)
    free(self->sup_indices), self->sup_indices = NULL;

  SU_DEBUG_9(("su_port_destroy() freed registrations\n"));

  su_home_zap(self->sup_home);

  SU_DEBUG_9(("su_port_destroy() returns\n"));

}

static void su_port_lock(su_port_t *self, char const *who)
{
  SU_PORT_LOCK(self, who);
}

static void su_port_unlock(su_port_t *self, char const *who)
{
  SU_PORT_UNLOCK(self, who);
}

static void su_port_incref(su_port_t *self, char const *who)
{
  SU_PORT_INCREF(self, who);
}

static void su_port_decref(su_port_t *self, int blocking, char const *who)
{
  if (blocking)
    SU_PORT_ZAPREF(self, who);
  else
    SU_PORT_DECREF(self, who);
}

static struct _GSource *su_port_gsource(su_port_t *self)
{
  return NULL;
}

#if SU_HAVE_MBOX
/** @internal Message box wakeup function. */
static int su_port_wakeup(su_root_magic_t *magic, /* NULL */
			  su_wait_t *w,
			  su_wakeup_arg_t *arg)
{
  char buf[32];
  su_socket_t s = *(su_socket_t *)arg;
  su_wait_events(w, s);
  recv(s, buf, sizeof(buf), 0);
  return 0;
}
#endif

/** @internal Send a message to the port. */
int su_port_send(su_port_t *self, su_msg_r rmsg)
{
  if (self) {
    int wakeup;

    SU_PORT_LOCK(self, "su_port_send");
    
    wakeup = self->sup_head == NULL;

    *self->sup_tail = rmsg[0]; rmsg[0] = NULL;
    self->sup_tail = &(*self->sup_tail)->sum_next;

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

    SU_PORT_UNLOCK(self, "su_port_send");

    return 0;
  }
  else {
    su_msg_destroy(rmsg);
    return -1;
  }
}

/** @internal
 * Execute the messages in the incoming queue until the queue is empty..
 *
 * @param self - pointer to a port object
 *
 * @retval Number of messages sent
 */
int su_port_getmsgs(su_port_t *self)
{
  int n = 0;

  if (self->sup_head) {
    su_msg_f f;
    su_msg_t *msg, *queue;

    SU_PORT_LOCK(self, "su_port_getmsgs");

    queue = self->sup_head;
    self->sup_tail = &self->sup_head;
    self->sup_head = NULL;

    SU_PORT_UNLOCK(self, "su_port_getmsgs");

    for (msg = queue; msg; msg = queue) {
      queue = msg->sum_next;
      msg->sum_next = NULL;

      f = msg->sum_func;
      if (f) 
	f(SU_ROOT_MAGIC(msg->sum_to->sut_root), &msg, msg->sum_data);
      su_msg_delivery_report(&msg);
      n++;
    }

    /* Check for wait events that may have been generated by messages */
    su_port_wait_events(self, 0);
  }

  return n;
}

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
 *   Positive index of the wait object, 
 *   or -1 upon an error.
 */
int su_port_register(su_port_t *self,
		     su_root_t *root, 
		     su_wait_t *wait, 
		     su_wakeup_f callback,
		     su_wakeup_arg_t *arg,
		     int priority)
{
  int i, j, n;

  assert(SU_PORT_OWN_THREAD(self));

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

      if (self->sup_size_waits == 0)
	indices[0] = -1;

      for (i = self->sup_size_waits + 1; i <= size; i++)
	indices[i] = -1 - i;
    }

    reverses = realloc(self->sup_reverses, size * sizeof(*waits));
    if (reverses) {
      for (i = self->sup_size_waits; i < size; i++)
	reverses[i] = -1;
      self->sup_reverses = reverses;
    }
      
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
	  reverses && waits && wait_cbs && wait_args && wait_tasks)) {
      return -1;
    }

    self->sup_size_waits = size;
  }

  i = -self->sup_indices[0]; assert(i <= self->sup_size_waits);

#if HAVE_EPOLL
  if (self->sup_epoll != -1) {
    struct epoll_event ev;

    ev.events = POLL2EPOLL(wait->events);
    ev.data.u64 = 0;
    ev.data.u32 = (uint32_t)i;

    if (epoll_ctl(self->sup_epoll, EPOLL_CTL_ADD, wait->fd, &ev) == -1) {
      SU_DEBUG_0(("EPOLL_CTL_ADD(%u, %u) failed: %s\n",
		  wait->fd, ev.events, strerror(errno)));
      return -1;
    }
  }
  else
#endif  
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
static
int su_port_deregister0(su_port_t *self, int i)
{
  int n, N, *indices, *reverses;

  indices = self->sup_indices;
  reverses = self->sup_reverses;

  n = indices[i]; assert(n >= 0);

#if HAVE_EPOLL
  if (self->sup_epoll != -1) {
    su_wait_t *wait = &self->sup_waits[n];
    struct epoll_event ev;

    ev.events = POLL2EPOLL(wait->events);
    ev.data.u64 = (uint64_t)0;
    ev.data.u32 = (uint32_t)i;

    if (epoll_ctl(self->sup_epoll, EPOLL_CTL_DEL, wait->fd, &ev) == -1) {
      SU_DEBUG_1(("su_port(%p): EPOLL_CTL_DEL(%u): %s\n", self, 
		  wait->fd, su_strerror(su_errno())));
    }
  }
#endif

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
 *  The function su_port_unregister() unregisters a su_wait_t object. The
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
 * @deprecated Use su_port_deregister() instead. 
 *
 * @return Nonzero index of the wait object, or -1 upon an error.
 */
int su_port_unregister(su_port_t *self,
		       su_root_t *root, 
		       su_wait_t *wait,	
		       su_wakeup_f callback, /* XXX - ignored */
		       su_wakeup_arg_t *arg)
{
  int n, N;

  assert(self);
  assert(SU_PORT_OWN_THREAD(self));

  N = self->sup_n_waits;

  for (n = 0; n < N; n++) {
    if (SU_WAIT_CMP(wait[0], self->sup_waits[n]) == 0) {
      return su_port_deregister0(self, self->sup_reverses[n]);
    }
  }

  su_seterrno(ENOENT);

  return -1;
}

/** Deregister a su_wait_t object.
 *  
 *  The function su_port_deregister() deregisters a su_wait_t registrattion. 
 *  The wait object, a callback function and a argument are removed from the
 *  port object.
 * 
 * @param self     - pointer to port object
 * @param i        - registration index
 * 
 * @return Index of the wait object, or -1 upon an error.
 */
int su_port_deregister(su_port_t *self, int i)
{
  su_wait_t wait[1] = { SU_WAIT_INIT };
  int retval;

  assert(self);
  assert(SU_PORT_OWN_THREAD(self));

  if (i <= 0 || i > self->sup_size_waits)
    return su_seterrno(EBADF);

  if (self->sup_indices[i] < 0)
    return su_seterrno(EBADF);
    
  retval = su_port_deregister0(self, i);

  su_wait_destroy(wait);

  return retval;
}


/** @internal
 * Unregister all su_wait_t objects.
 *
 * The function su_port_unregister_all() unregisters all su_wait_t objects
 * and destroys all queued timers associated with given root object.
 * 
 * @param  self     - pointer to port object
 * @param  root     - pointer to root object
 * 
 * @return Number of wait objects removed.
 */
int su_port_unregister_all(su_port_t *self, 
			   su_root_t *root)
{
  int i, j, index, N;
  int             *indices, *reverses;
  su_wait_t       *waits;
  su_wakeup_f     *wait_cbs;
  su_wakeup_arg_t**wait_args;
  su_root_t      **wait_roots;

  assert(SU_PORT_OWN_THREAD(self));

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
#if HAVE_EPOLL
      if (self->sup_epoll != -1) {
	int fd = waits[i].fd;
	struct epoll_event ev;

	ev.events = POLL2EPOLL(waits[i].events);
	ev.data.u64 = (uint64_t)0;
	ev.data.u32 = (uint32_t)index;

	if (epoll_ctl(self->sup_epoll, EPOLL_CTL_DEL, fd, &ev) == -1) {
	  SU_DEBUG_1(("EPOLL_CTL_DEL(%u): %s\n", 
		      waits[i].fd, su_strerror(su_errno())));
	}
      }
#endif
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
 * The function su_port_eventmask() sets the mask describing events that can
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
int su_port_eventmask(su_port_t *self, int index, int socket, int events)
{
  int n;
  assert(self);
  assert(SU_PORT_OWN_THREAD(self));

  if (index <= 0 || index > self->sup_size_waits)
    return su_seterrno(EBADF);
  n = self->sup_indices[index];
  if (n < 0)
    return su_seterrno(EBADF);

#if HAVE_EPOLL
  if (self->sup_epoll != -1) {
    su_wait_t *wait = &self->sup_waits[n];
    struct epoll_event ev;

    wait->events = events;

    ev.events = POLL2EPOLL(events);
    ev.data.u64 = (uint64_t)0;
    ev.data.u32 = (uint32_t)index;

    if (epoll_ctl(self->sup_epoll, EPOLL_CTL_MOD, wait->fd, &ev) == -1) {
      SU_DEBUG_1(("su_port(%p): EPOLL_CTL_MOD(%u): %s\n", self, 
		  wait->fd, su_strerror(su_errno())));
      return -1;
    }
  }
#endif

  return su_wait_mask(&self->sup_waits[n], socket, events);
}

#if 0
/** @internal
 *
 * Copies the su_wait_t objects from the port. The number of wait objects
 * can be found out by calling su_port_query() with @a n_waits as zero.
 * 
 * @note This function is called only by friends.
 *
 * @param self     - pointer to port object
 * @param waits    - pointer to array to which wait objects are copied
 * @param n_waits  - number of wait objects fitting in array waits
 *
 * @return Number of wait objects, or 0 upon an error.
 */
unsigned su_port_query(su_port_t *self, su_wait_t *waits, unsigned n_waits)
{
  unsigned n;

  assert(SU_PORT_OWN_THREAD(self));

  n = self->sup_n_waits;

  if (n_waits != 0) {
    if (waits && n_waits >= n)
      memcpy(waits, self->sup_waits, n * sizeof(*waits));
    else
      n = 0;
  }

  return n;
}
#endif

/** @internal Enable multishot mode.
 *
 * The function su_port_multishot() enables, disables or queries the
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
int su_port_multishot(su_port_t *self, int multishot)
{
  if (multishot < 0)
    return self->sup_multishot;
  else if (multishot == 0 || multishot == 1)
    return self->sup_multishot = multishot;
  else 
    return (errno = EINVAL), -1;
}

/** @internal Enable threadsafe operation. */
static
int su_port_threadsafe(su_port_t *port)
{
  return su_home_threadsafe(port->sup_home);
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
void su_port_run(su_port_t *self)
{
  su_duration_t tout = 0;

  assert(SU_PORT_OWN_THREAD(self));

  for (self->sup_running = 1; self->sup_running;) {
    tout = 2000;

    if (self->sup_prepoll)
      self->sup_prepoll(self->sup_pp_magic, self->sup_pp_root);

    if (self->sup_head)
      su_port_getmsgs(self);

    if (self->sup_timers)
      su_timer_expire(&self->sup_timers, &tout, su_now());

    if (!self->sup_running)
      break;

    if (self->sup_head)      /* if there are messages do a quick wait */
      tout = 0;

    su_port_wait_events(self, tout);
  }
}

#if tuning
/* This version can help tuning... */
void su_port_run_tune(su_port_t *self)
{
  int i;
  int timers = 0, messages = 0, events = 0;
  su_duration_t tout = 0, tout0;
  su_time_t started = su_now(), woken = started, bedtime = woken;

  assert(SU_PORT_OWN_THREAD(self));

  for (self->sup_running = 1; self->sup_running;) {
    tout0 = tout, tout = 2000;

    timers = 0, messages = 0;

    if (self->sup_prepoll)
      self->sup_prepoll(self->sup_pp_magic, self->sup_pp_root);

    if (self->sup_head)
      messages = su_port_getmsgs(self);

    if (self->sup_timers)
      timers = su_timer_expire(&self->sup_timers, &tout, su_now());

    if (!self->sup_running)
      break;

    if (self->sup_head)      /* if there are messages do a quick wait */
      tout = 0;

    bedtime = su_now();

    events = su_port_wait_events(self, tout);

    woken = su_now();

    if (messages || timers || events)
      SU_DEBUG_1(("su_port_run(%p): %.6f: %u messages %u timers %u "
		  "events slept %.6f/%.3f\n",
		  self, su_time_diff(woken, started), messages, timers, events,
		  su_time_diff(woken, bedtime), tout0 * 1e-3));

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
void su_port_break(su_port_t *self)
{
  self->sup_running = 0; 
}

/** @internal
 * The function @c su_port_wait_events() is used to poll() for wait objects
 *
 * @param self     pointer to port
 * @param tout     timeout in milliseconds
 *
 * @return number of events handled
 */
static
int su_port_wait_events(su_port_t *self, su_duration_t tout)
{
  int i, events = 0;
  su_wait_t *waits = self->sup_waits;
  int n = self->sup_n_waits;
  su_root_t *root;
#if HAVE_POLL
  unsigned version = self->sup_registers;
#endif
#if HAVE_EPOLL
  
  if (self->sup_epoll != -1) {
    int const M = 4;
    struct epoll_event ev[M];
    int j, index;
    int *indices = self->sup_indices;
    
    n = epoll_wait(self->sup_epoll, ev, self->sup_multishot ? M : 1, tout);

    assert(n <= M);

    for (j = 0; j < n; j++) {
      su_root_t *root;
      su_root_magic_t *magic;

      if (!ev[j].events || ev[j].data.u32 > INDEX_MAX)
	continue;
      index = (int)ev[j].data.u32;
      assert(index > 0 && index <= self->sup_size_waits);
      i = indices[index]; assert(i >= 0 && i <= self->sup_n_waits);
      root = self->sup_wait_roots[i];
      magic = root ? su_root_magic(root) : NULL;
      waits[i].revents = ev[j].events;
      self->sup_wait_cbs[i](magic, &waits[i], self->sup_wait_args[i]);
      events++;
      /* Callback function used su_register()/su_deregister() */
      if (version != self->sup_registers)
	break;
    }
    
    return n < 0 ? n : events;
  }
#endif

  i = su_wait(waits, (unsigned)n, tout);

  if (i >= 0 && i < n) {
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

/** @internal 
 * Used to check wait events in callbacks that take lots of time
 *
 * This function does a timeout 0 poll() and runs wait objects.
 *
 * @param port     pointer to port
 *
 * @return number of events handled
 */
static
int su_port_yield(su_port_t *port)
{
  return su_port_wait_events(port, 0);
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
su_duration_t su_port_step(su_port_t *self, su_duration_t tout)
{
  su_time_t now = su_now();

  assert(SU_PORT_OWN_THREAD(self));

  if (self->sup_prepoll)
    self->sup_prepoll(self->sup_pp_magic, self->sup_pp_root);

  if (self->sup_head)
    su_port_getmsgs(self);

  if (self->sup_timers)
    su_timer_expire(&self->sup_timers, &tout, now);

  /* if there are messages do a quick wait */
  if (self->sup_head)
    tout = 0;

  if (su_port_wait_events(self, tout))
    tout = 0;
  else
    tout = SU_WAIT_FOREVER;

  if (self->sup_head)
    su_port_getmsgs(self);

  if (self->sup_timers)
    su_timer_expire(&self->sup_timers, &tout, su_now());

  if (self->sup_head)
    tout = 0;

  return tout;
}


/** @internal
 * Checks if the calling thread owns the port object.
 *
 * @param self pointer to a port object
 *
 * @retval true (nonzero) if the calling thread owns the port,
 * @retval false (zero) otherwise.
 */
int su_port_own_thread(su_port_t const *self)
{
  return self == NULL || SU_PORT_OWN_THREAD(self);
}

#if 0
/** @internal
 *  Prints out the contents of the port.
 *
 * @param self pointer to a port
 * @param f    pointer to a file (if @c NULL, uses @c stdout).
 */
void su_port_dump(su_port_t const *self, FILE *f)
{
  int i;
#define IS_WAIT_IN(x) (((x)->events & SU_WAIT_IN) ? "IN" : "")
#define IS_WAIT_OUT(x) (((x)->events & SU_WAIT_OUT) ? "OUT" : "")
#define IS_WAIT_ACCEPT(x) (((x)->events & SU_WAIT_ACCEPT) ? "ACCEPT" : "")

  if (f == NULL)
    f = stdout;

  fprintf(f, "su_port_t at %p:\n", self);
  fprintf(f, "\tport is%s running\n", self->sup_running ? "" : "not ");
#if SU_HAVE_PTHREADS
  fprintf(f, "\tport tid %p\n", (void *)self->sup_tid);
#endif
#if SU_HAVE_MBOX
  fprintf(f, "\tport mbox %d (%s%s%s)\n", self->sup_mbox[0],
	  IS_WAIT_IN(&self->sup_mbox_wait),
	  IS_WAIT_OUT(&self->sup_mbox_wait),
	  IS_WAIT_ACCEPT(&self->sup_mbox_wait));
#endif
  fprintf(f, "\t%d wait objects\n", self->sup_n_waits);
  for (i = 0; i < self->sup_n_waits; i++) {
    
  }
}

#endif

/* =========================================================================
 * Pre-poll() callback
 */

int su_port_add_prepoll(su_port_t *port,
			su_root_t *root, 
			su_prepoll_f *callback, 
			su_prepoll_magic_t *magic)
{
  if (port->sup_prepoll)
    return -1;

  port->sup_prepoll = callback;
  port->sup_pp_magic = magic;
  port->sup_pp_root = root;

  return 0;
}

int su_port_remove_prepoll(su_port_t *port,
			   su_root_t *root)
{
  if (port->sup_pp_root != root)
    return -1;

  port->sup_prepoll = NULL;
  port->sup_pp_magic = NULL;
  port->sup_pp_root = NULL;

  return 0;
}

/* =========================================================================
 * Timers
 */

static
su_timer_t **su_port_timers(su_port_t *self)
{
  return &self->sup_timers;
}
