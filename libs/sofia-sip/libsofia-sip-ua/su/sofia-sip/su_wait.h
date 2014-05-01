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

#ifndef SU_WAIT_H
/** Defined when <sofia-sip/su_wait.h> has been included. */
#define SU_WAIT_H

/**@ingroup su_wait
 * @file sofia-sip/su_wait.h Syncronization and threading interface.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * @author Martti Mela <Martti.Mela@nokia.com>
 *
 * @date Created: Tue Sep 14 15:51:04 1999 ppessi
 */

/* ---------------------------------------------------------------------- */
/* Includes */

#ifndef SU_H
#include "sofia-sip/su.h"
#endif

#ifndef SU_TIME_H
#include "sofia-sip/su_time.h"
#endif

#if SU_HAVE_POLL
#include <poll.h>
#endif

SOFIA_BEGIN_DECLS

/* ---------------------------------------------------------------------- */
/* Constants */

#if SU_HAVE_KQUEUE
/** Compare wait object. @HI */
#define SU_WAIT_CMP(x, y) \
 (((x).ident - (y).ident) ? ((x).ident - (y).ident) : ((x).flags - (y).flags))

/** Incoming data is available on socket. @HI */
#define SU_WAIT_IN      (EVFILT_READ)
/** Data can be sent on socket. @HI */
#define SU_WAIT_OUT     (EVFILT_WRITE)
/** Socket is connected. @HI */
#define SU_WAIT_CONNECT (EVFILT_WRITE)
/** An error occurred on socket. @HI */
#define SU_WAIT_ERR     (EV_ERROR)
/** The socket connection was closed. @HI */
#define SU_WAIT_HUP     (EV_EOF)
/** A listening socket accepted a new connection. @HI */
#define SU_WAIT_ACCEPT  (EVFILT_READ)

/** No timeout for su_wait(). */
#define SU_WAIT_FOREVER (-1)
/** The return value of su_wait() if timeout occurred. */
#define SU_WAIT_TIMEOUT (-2)

/** Initializer for a wait object. @HI */
#define SU_WAIT_INIT    { INVALID_SOCKET, 0, 0, 0, 0, NULL }

/** Maximum number of sources supported by su_wait() */
#define SU_WAIT_MAX    (0x7fffffff)

#elif SU_HAVE_POLL || DOCUMENTATION_ONLY
/** Compare wait object. @HI */
#define SU_WAIT_CMP(x, y) \
 (((x).fd - (y).fd) ? ((x).fd - (y).fd) : ((x).events - (y).events))

/** Incoming data is available on socket. @HI */
#define SU_WAIT_IN      (POLLIN)
/** Data can be sent on socket. @HI */
#define SU_WAIT_OUT     (POLLOUT)
/** Socket is connected. @HI */
#define SU_WAIT_CONNECT (POLLOUT)
/** An error occurred on socket. @HI */
#define SU_WAIT_ERR     (POLLERR)
/** The socket connection was closed. @HI */
#define SU_WAIT_HUP     (POLLHUP)
/** A listening socket accepted a new connection. @HI */
#define SU_WAIT_ACCEPT  (POLLIN)

/** No timeout for su_wait(). */
#define SU_WAIT_FOREVER (-1)
/** The return value of su_wait() if timeout occurred. */
#define SU_WAIT_TIMEOUT (-2)

/** Initializer for a wait object. @HI */
#define SU_WAIT_INIT    { INVALID_SOCKET, 0, 0 }

/** Maximum number of sources supported by su_wait() */
#define SU_WAIT_MAX    (0x7fffffff)

#elif SU_HAVE_WINSOCK

#define SU_WAIT_CMP(x, y) ((intptr_t)(x) - (intptr_t)(y))

#define SU_WAIT_IN      (FD_READ)
#define SU_WAIT_OUT     (FD_WRITE)
#define SU_WAIT_CONNECT (FD_CONNECT)
#define SU_WAIT_ERR     (0)	/* let's get it on */
#define SU_WAIT_HUP     (FD_CLOSE)
#define SU_WAIT_ACCEPT  (FD_ACCEPT)

#define SU_WAIT_FOREVER (WSA_INFINITE)
#define SU_WAIT_TIMEOUT (WSA_WAIT_TIMEOUT)

#define SU_WAIT_INIT    NULL

#define SU_WAIT_MAX    (64)

#else
/* If nothing works, try these */

#define	POLLIN		0x001
#define	POLLPRI		0x002
#define	POLLOUT		0x004

#ifdef __USE_XOPEN
#define	POLLRDNORM	0x040
#define	POLLRDBAND	0x080
#define	POLLWRNORM	0x100
#define	POLLWRBAND	0x200
#endif

/* These for pollfd.revents */
#define POLLERR         0x008
#define POLLHUP         0x010
#define POLLNVAL        0x020

#define SU_WAIT_CMP(x, y) \
 (((x).fd - (y).fd) ? ((x).fd - (y).fd) : ((x).events - (y).events))

#define SU_WAIT_IN      POLLIN
#define SU_WAIT_OUT     POLLOUT
#define SU_WAIT_CONNECT POLLOUT
#define SU_WAIT_ERR     POLLERR
#define SU_WAIT_HUP     POLLHUP
#define SU_WAIT_ACCEPT  POLLIN
#define SU_WAIT_FOREVER (-1)
#define SU_WAIT_TIMEOUT (-2)

#define SU_WAIT_INIT    { INVALID_SOCKET, 0, 0 }

/** Maximum number of sources supported by su_wait() */
#define SU_WAIT_MAX    (0x7fffffff)

#endif

/* ---------------------------------------------------------------------- */
/* Types */

/** Wait object. */
#if SU_HAVE_KQUEUE
typedef struct kevent su_wait_t;
#elif SU_HAVE_POLL
typedef struct pollfd su_wait_t;
#elif SU_HAVE_WINSOCK
typedef HANDLE su_wait_t;
#else
/* typedef struct os_specific su_wait_t; */
typedef struct pollfd su_wait_t;
struct pollfd {
  su_socket_t fd;   /* file descriptor */
  short events;     /* requested events */
  short revents;    /* returned events */
};


/* Type used for the number of file descriptors.  */
typedef unsigned long int nfds_t;

/* Poll the file descriptors described by the NFDS structures starting at
   FDS.  If TIMEOUT is nonzero and not -1, allow TIMEOUT milliseconds for
   an event to occur; if TIMEOUT is -1, block until an event occurs.
   Returns the number of file descriptors with events, zero if timed out,
   or -1 for errors.  */
int poll (struct pollfd *__fds, nfds_t __nfds, int __timeout);

#endif

/* Used by AD */
typedef int su_success_t;

/* ---------------------------------------------------------------------- */

/** <a href="#su_root_t">Root object</a> type. */
typedef struct su_root_s su_root_t;

#ifndef SU_ROOT_MAGIC_T
/**Default type of application context for <a href="#su_root_t">su_root_t</a>.
 *
 * Application may define the typedef ::su_root_magic_t to appropriate type
 * by defining macro SU_ROOT_MAGIC_T before including <sofia-sip/su_wait.h>, for
 * example,
 * @code
 * #define SU_ROOT_MAGIC_T struct context
f * #include <sofia-sip/su_wait.h>
 * @endcode
 */
#define SU_ROOT_MAGIC_T void
#endif

/** <a href="#su_root_t">Root context</a> pointer type.
 *
 * Application may define the typedef ::su_root_magic_t to appropriate type
 * by defining macro SU_ROOT_MAGIC_T () before including <sofia-sip/su_wait.h>, for
 * example,
 * @code
 * #define SU_ROOT_MAGIC_T struct context
 * #include <sofia-sip/su_wait.h>
 * @endcode
 */
typedef SU_ROOT_MAGIC_T su_root_magic_t;

#ifndef SU_WAKEUP_ARG_T
/**Default type of @link ::su_wakeup_f wakeup function @endlink
 * @link ::su_wakeup_arg_t argument type @endlink.
 *
 * The application can define the typedef ::su_wakeup_arg_t by defining
 * the SU_WAKEUP_ARG_T () before including <sofia-sip/su_wait.h>, for example,
 * @code
 * #define SU_WAKEUP_ARG_T struct transport
 * #include <sofia-sip/su_wait.h>
 * @endcode
 */
#define SU_WAKEUP_ARG_T void
#endif

/** @link ::su_wakeup_f Wakeup callback @endlink argument type.
 *
 * The application can define the typedef ::su_wakeup_arg_t by defining
 * the SU_WAKEUP_ARG_T () before including <sofia-sip/su_wait.h>, for example,
 * @code
 * #define SU_WAKEUP_ARG_T struct transport
 * #include <sofia-sip/su_wait.h>
 * @endcode
 */
typedef SU_WAKEUP_ARG_T su_wakeup_arg_t;

/** Wakeup callback function pointer type.
 *
 * Whenever a registered wait object receives an event, the @link
 * ::su_wakeup_f callback function @endlink is invoked.
 */
typedef int (*su_wakeup_f)(su_root_magic_t *,
			   su_wait_t *,
			   su_wakeup_arg_t *arg);

enum {
  su_pri_normal,		/**< Normal priority */
  su_pri_first,			/**< Elevated priority */
  su_pri_realtime		/**< Real-time priority */
};

struct _GSource;

/** Hint for number of registered fds in su_root */
SOFIAPUBVAR int su_root_size_hint;

/* ---------------------------------------------------------------------- */
/* Pre-poll callback */

#ifndef SU_PREPOLL_MAGIC_T
/**Default type of application context for prepoll function.
 *
 * Application may define the typedef ::su_prepoll_magic_t to appropriate type
 * by defining macro #SU_PREPOLL_MAGIC_T before including <sofia-sip/su_wait.h>, for
 * example,
 * @code
 * #define SU_PREPOLL_MAGIC_T struct context
 * #include <sofia-sip/su_wait.h>
 * @endcode
 */
#define SU_PREPOLL_MAGIC_T void
#endif

/** <a href="#su_root_t">Root context</a> pointer type.
 *
 * Application may define the typedef ::su_prepoll_magic_t to appropriate type
 * by defining macro #SU_PREPOLL_MAGIC_T before including <sofia-sip/su_wait.h>, for
 * example,
 * @code
 * #define SU_PREPOLL_MAGIC_T struct context
 * #include <sofia-sip/su_wait.h>
 * @endcode
 */
typedef SU_PREPOLL_MAGIC_T su_prepoll_magic_t;


/** Pre-poll callback function prototype.
 *
 *
 */
typedef void su_prepoll_f(su_prepoll_magic_t *, su_root_t *);

/* ---------------------------------------------------------------------- */

/* Timers */
#ifdef SU_TIMER_T
#error SU_TIMER_T defined
#endif

#ifndef SU_TIMER_ARG_T
/** Default type of timer expiration callback function argument type.
 * Application may define this to appropriate type before including
 * <sofia-sip/su_wait.h>. */
#define SU_TIMER_ARG_T void
#endif

/** Timer object type. */
typedef struct su_timer_s su_timer_t;

/** Timer callback argument type. */
typedef SU_TIMER_ARG_T su_timer_arg_t;

/** Timeout function type. */
typedef void (*su_timer_f)(su_root_magic_t *magic,
			   su_timer_t *t,
			   su_timer_arg_t *arg);

#ifndef SU_TIMER_QUEUE_T
#define SU_TIMER_QUEUE_T su_timer_t *
#endif

typedef SU_TIMER_QUEUE_T su_timer_queue_t;

/* ---------------------------------------------------------------------- */

/* Tasks */

/** Port type. */
typedef struct su_port_s su_port_t;

typedef struct { su_port_t *sut_port; su_root_t *sut_root; } _su_task_t;

/** Task reference type. */
typedef _su_task_t su_task_r[1];

/** Initializer for a task reference. @HI */
#define SU_TASK_R_INIT  {{ NULL, NULL }}

/* This must be used instead of su_task_r as return value type. */
typedef _su_task_t const *_su_task_r;

/* ---------------------------------------------------------------------- */

/* Messages */
#ifndef SU_MSG_ARG_T
/** Default type of su_msg_t message data.  Application may define this to
 * appropriate type before including <sofia-sip/su_wait.h>.
 */
#define SU_MSG_ARG_T void
#endif

/** Message argument type. */
typedef SU_MSG_ARG_T su_msg_arg_t;

/** Message type. */
typedef struct su_msg_s su_msg_t;

/** Message reference type. */
typedef su_msg_t *su_msg_r[1];

/** Constant reference to su_msg.
 *
 * @note The reference is constant, but the message can be modified.
 */
typedef su_msg_t * const su_msg_cr[1];

/** Initializer for a message reference. @HI */
#define SU_MSG_R_INIT   { NULL }

/** Message delivery function type. */
typedef void su_msg_function(su_root_magic_t *magic,
			     su_msg_r msg,
			     su_msg_arg_t *arg);

/** Message deinitializer function type. @NEW_1_12_8 */
typedef void su_msg_deinit_function(su_msg_arg_t *arg);

/** Message delivery function pointer type. */
typedef su_msg_function *su_msg_f;


/* ---------------------------------------------------------------------- */

/* Clones */
#ifndef SU_CLONE_T
#define SU_CLONE_T struct su_clone_s
#endif

/** Clone reference. */
typedef SU_CLONE_T *su_clone_r[1];

/** Clone reference initializer. */
#define SU_CLONE_R_INIT  {NULL}

/** Clone initialization function type. */
typedef int (*su_root_init_f)(su_root_t *, su_root_magic_t *);

/** Clone finalization function type. */
typedef void (*su_root_deinit_f)(su_root_t *, su_root_magic_t *);

/* ---------------------------------------------------------------------- */
/* Functions */

/* Wait */
SOFIAPUBFUN void su_wait_init(su_wait_t dst[1]);
SOFIAPUBFUN int su_wait_create(su_wait_t *dst, su_socket_t s, int events);
SOFIAPUBFUN int su_wait_destroy(su_wait_t *dst);
SOFIAPUBFUN int su_wait(su_wait_t waits[], unsigned n, su_duration_t timeout);
SOFIAPUBFUN int su_wait_events(su_wait_t *wait, su_socket_t s);
SOFIAPUBFUN int su_wait_mask(su_wait_t *dst, su_socket_t s, int events);

#if !HAVE_WIN32 && (SU_HAVE_POLL || HAVE_SELECT)
su_inline
su_socket_t su_wait_socket(su_wait_t *wait)
{
#if SU_HAVE_KQUEUE
  return wait->ident;
#else
  return wait->fd;
#endif
}
#endif

/* Root */
SOFIAPUBFUN su_root_t *su_root_create(su_root_magic_t *magic)
  __attribute__((__malloc__));
SOFIAPUBFUN void su_root_destroy(su_root_t*);
SOFIAPUBFUN char const *su_root_name(su_root_t *self);
SOFIAPUBFUN int su_root_set_magic(su_root_t *self, su_root_magic_t *magic);
SOFIAPUBFUN su_root_magic_t *su_root_magic(su_root_t *root);
SOFIAPUBFUN int su_root_register(su_root_t*, su_wait_t *,
				 su_wakeup_f, su_wakeup_arg_t *,
				 int priority);
/* This is slow. Deprecated. */
SOFIAPUBFUN int su_root_unregister(su_root_t*, su_wait_t *,
				   su_wakeup_f, su_wakeup_arg_t*);
SOFIAPUBFUN int su_root_deregister(su_root_t*, int);
SOFIAPUBFUN int su_root_eventmask(su_root_t *,
				  int index, int socket, int events);
SOFIAPUBFUN su_duration_t su_root_step(su_root_t *root, su_duration_t timeout);
SOFIAPUBFUN su_duration_t su_root_sleep(su_root_t *root, su_duration_t);
SOFIAPUBFUN int su_root_multishot(su_root_t *root, int multishot);
SOFIAPUBFUN void su_root_run(su_root_t *root);
SOFIAPUBFUN void su_root_break(su_root_t *root);
SOFIAPUBFUN _su_task_r su_root_task(su_root_t const *root);
SOFIAPUBFUN _su_task_r su_root_parent(su_root_t const *root);

SOFIAPUBFUN int su_root_add_prepoll(su_root_t *root,
				    su_prepoll_f *,
				    su_prepoll_magic_t *);
SOFIAPUBFUN int su_root_remove_prepoll(su_root_t *root);

SOFIAPUBFUN struct _GSource *su_root_gsource(su_root_t *self);

SOFIAPUBFUN int su_root_yield(su_root_t *root);

SOFIAPUBFUN int su_root_release(su_root_t *root);
SOFIAPUBFUN int su_root_obtain(su_root_t *root);
SOFIAPUBFUN int su_root_has_thread(su_root_t *root);

SOFIAPUBFUN int su_root_set_max_defer(su_root_t *, su_duration_t max_defer);
SOFIAPUBFUN su_duration_t su_root_get_max_defer(su_root_t const *self);

SOFIAPUBFUN su_root_t *su_root_clone(su_root_t *root, su_root_magic_t *magic)
  __attribute__((__malloc__));

/* Timers */
SOFIAPUBFUN su_timer_t *su_timer_create(su_task_r const, su_duration_t msec)
     __attribute__((__malloc__));
SOFIAPUBFUN void su_timer_destroy(su_timer_t *);
SOFIAPUBFUN int su_timer_is_set(su_timer_t const *t); /* 1.12.11 */
SOFIAPUBFUN su_time_t su_timer_latest(su_timer_t const *t);
SOFIAPUBFUN int su_timer_set(su_timer_t *, su_timer_f, su_timer_arg_t *);
SOFIAPUBFUN int su_timer_set_interval(su_timer_t *t, su_timer_f,
				      su_timer_arg_t *, su_duration_t);
SOFIAPUBFUN int su_timer_set_at(su_timer_t *, su_timer_f,
				su_timer_arg_t *, su_time_t);
SOFIAPUBFUN int su_timer_run(su_timer_t *, su_timer_f, su_timer_arg_t *);
SOFIAPUBFUN int su_timer_set_for_ever(su_timer_t *, su_timer_f,
				      su_timer_arg_t *);
SOFIAPUBFUN int su_timer_reset(su_timer_t *);

SOFIAPUBFUN su_root_t *su_timer_root(su_timer_t const *);

SOFIAPUBFUN int su_timer_expire(su_timer_queue_t * const,
				su_duration_t *tout,
				su_time_t now);
SOFIAPUBFUN int su_timer_deferrable(su_timer_t *t, int value); /* 1.12.11 */

/* Tasks */

SOFIAPUBVAR su_task_r const su_task_null;

SOFIAPUBFUN _su_task_r su_task_init(su_task_r task);
SOFIAPUBFUN void su_task_deinit(su_task_r task);

SOFIAPUBFUN void su_task_copy(su_task_r dst, su_task_r const src);
SOFIAPUBFUN void su_task_move(su_task_r dst, su_task_r src);
SOFIAPUBFUN int su_task_cmp(su_task_r const, su_task_r const);
SOFIAPUBFUN int su_task_is_running(su_task_r const);

SOFIAPUBFUN su_root_t *su_task_root(su_task_r const self);
SOFIAPUBFUN su_timer_queue_t *su_task_timers(su_task_r const self);
SOFIAPUBFUN su_timer_queue_t *su_task_deferrable(su_task_r const task);

SOFIAPUBFUN int su_task_wakeup(su_task_r const task);

SOFIAPUBFUN int su_task_execute(su_task_r const task,
				int (*function)(void *), void *arg,
				int *return_value);

/* Messages */
SOFIAPUBFUN int su_msg_create(su_msg_r msg,
			      su_task_r const to, su_task_r const from,
			      su_msg_f wakeup, isize_t size);
SOFIAPUBFUN int su_msg_report(su_msg_r msg, su_msg_f report);
SOFIAPUBFUN int su_msg_deinitializer(su_msg_r msg, su_msg_deinit_function *);
SOFIAPUBFUN int su_msg_reply(su_msg_r reply, su_msg_cr msg,
			     su_msg_f wakeup, isize_t size);
SOFIAPUBFUN void su_msg_destroy(su_msg_r msg);
SOFIAPUBFUN void su_msg_save(su_msg_r msg, su_msg_r msg0);
SOFIAPUBFUN void su_msg_remove_refs(su_msg_cr msg);
SOFIAPUBFUN su_msg_arg_t *su_msg_data(su_msg_cr msg);
SOFIAPUBFUN isize_t su_msg_size(su_msg_cr msg);
SOFIAPUBFUN _su_task_r su_msg_from(su_msg_cr msg);
SOFIAPUBFUN _su_task_r su_msg_to(su_msg_cr msg);
SOFIAPUBFUN int su_msg_send(su_msg_r msg);

SOFIAPUBFUN int su_msg_new(su_msg_r msg, size_t size);
SOFIAPUBFUN int su_msg_send_to(su_msg_r msg,
			       su_task_r const to,
			       su_msg_f wakeup);

/** Does reference contain a message? */
#if SU_HAVE_INLINE
static SU_INLINE
int su_msg_is_non_null(su_msg_cr msg)
{
  return msg && *msg != NULL;
}
#else
#define su_msg_is_non_null(msg) ((msg) && (*(msg)) != NULL)
#endif

/* Clones */
SOFIAPUBFUN int su_root_threading(su_root_t *self, int enable);
SOFIAPUBFUN int su_clone_start(su_root_t *root,
			       su_clone_r,
			       su_root_magic_t *magic,
			       su_root_init_f,
			       su_root_deinit_f);
SOFIAPUBFUN _su_task_r su_clone_task(su_clone_r);
SOFIAPUBFUN void su_clone_forget(su_clone_r);
SOFIAPUBFUN void su_clone_stop(su_clone_r);
SOFIAPUBFUN void su_clone_wait(su_root_t *root, su_clone_r clone);

SOFIAPUBFUN int su_clone_pause(su_clone_r);
SOFIAPUBFUN int su_clone_resume(su_clone_r);

/* ---------------------------------------------------------------------- */
/* Different su_root_t implementations */

typedef su_port_t *su_port_create_f(void);
typedef int su_clone_start_f(su_root_t *parent,
			    su_clone_r return_clone,
			    su_root_magic_t *magic,
			    su_root_init_f init,
			    su_root_deinit_f deinit);

SOFIAPUBFUN void su_port_prefer(su_port_create_f *f, su_clone_start_f *);

SOFIAPUBFUN su_port_create_f su_default_port_create;
SOFIAPUBFUN su_port_create_f su_epoll_port_create;
SOFIAPUBFUN su_port_create_f su_poll_port_create;
SOFIAPUBFUN su_port_create_f su_wsaevent_port_create;
SOFIAPUBFUN su_port_create_f su_select_port_create;
SOFIAPUBFUN su_port_create_f su_kqueue_port_create;
SOFIAPUBFUN su_port_create_f su_devpoll_port_create;

SOFIAPUBFUN su_clone_start_f su_default_clone_start;
SOFIAPUBFUN su_clone_start_f su_epoll_clone_start;
SOFIAPUBFUN su_clone_start_f su_poll_clone_start;
SOFIAPUBFUN su_clone_start_f su_wsaevent_clone_start;
SOFIAPUBFUN su_clone_start_f su_select_clone_start;
SOFIAPUBFUN su_clone_start_f su_kqueue_clone_start;
SOFIAPUBFUN su_clone_start_f su_devpoll_clone_start;

SOFIA_END_DECLS

#endif /* SU_WAIT_H */
