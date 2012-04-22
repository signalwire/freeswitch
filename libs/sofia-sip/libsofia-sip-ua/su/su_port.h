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

#ifndef SU_PORT_H
/** Defined when <su_port.h> has been included. */
#define SU_PORT_H

/**@internal @file su_port.h
 *
 * @brief Internal OS-independent syncronization interface.
 *
 * This looks like the "reactor" pattern.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 *
 * @date Created: Fri May 12 14:13:34 2000 ppessi
 */

#ifndef SU_MSG_ARG_T
#define SU_MSG_ARG_T union { char anoymous[4]; }
#endif

#ifndef SU_WAIT_H
#include "sofia-sip/su_wait.h"
#endif

#ifndef SU_MODULE_DEBUG_H
#include "su_module_debug.h"
#endif

#ifndef SU_ALLOC_H
#include <sofia-sip/su_alloc.h>
#endif

#include <assert.h>

#define SU_WAIT_MIN    (16)

SOFIA_BEGIN_DECLS

/** @internal Message */
struct su_msg_s {
  size_t         sum_size;
  su_msg_t      *sum_next;
  su_task_r      sum_to;
  su_task_r      sum_from;
  su_msg_f       sum_func;
  su_msg_f       sum_report;
  su_msg_deinit_function *sum_deinit;
  su_msg_arg_t   sum_data[1];		/* minimum size, may be extended */
};

struct _GSource;

/** @internal Root structure */
struct su_root_s {
  int              sur_size;
  su_root_magic_t *sur_magic;
  su_root_deinit_f sur_deinit;
  su_task_r        sur_task;
  su_task_r        sur_parent;
  unsigned         sur_threading : 1;
  unsigned         sur_deiniting : 1;
};

#define SU_ROOT_MAGIC(r) ((r) ? (r)->sur_magic : NULL)

enum su_port_thread_op {
  su_port_thread_op_is_obtained,
  su_port_thread_op_release,
  su_port_thread_op_obtain
};

/** @internal Virtual function table for port */
typedef struct su_port_vtable {
  unsigned su_vtable_size;
  void (*su_port_lock)(su_port_t *port, char const *who);
  void (*su_port_unlock)(su_port_t *port, char const *who);
  void (*su_port_incref)(su_port_t *port, char const *who);
  void (*su_port_decref)(su_port_t *port, int block, char const *who);
  struct _GSource *(*su_port_gsource)(su_port_t *port);
  int (*su_port_send)(su_port_t *self, su_msg_r rmsg);
  int (*su_port_register)(su_port_t *self,
			  su_root_t *root,
			  su_wait_t *wait,
			  su_wakeup_f callback,
			  su_wakeup_arg_t *arg,
			  int priority);
  int (*su_port_unregister)(su_port_t *port,
			    su_root_t *root,
			    su_wait_t *wait,
			    su_wakeup_f callback,
			    su_wakeup_arg_t *arg);
  int (*su_port_deregister)(su_port_t *self, int i);
  int (*su_port_unregister_all)(su_port_t *self,
			     su_root_t *root);
  int (*su_port_eventmask)(su_port_t *self, int index, int socket, int events);
  void (*su_port_run)(su_port_t *self);
  void (*su_port_break)(su_port_t *self);
  su_duration_t (*su_port_step)(su_port_t *self, su_duration_t tout);

  /* Reused slot */
  int (*su_port_thread)(su_port_t *port, enum su_port_thread_op op);

  int (*su_port_add_prepoll)(su_port_t *port,
			     su_root_t *root,
			     su_prepoll_f *,
			     su_prepoll_magic_t *);

  int (*su_port_remove_prepoll)(su_port_t *port,
				su_root_t *root);

  su_timer_queue_t *(*su_port_timers)(su_port_t *port);

  int (*su_port_multishot)(su_port_t *port, int multishot);

  /* Extension from >= 1.12.4 */
  int (*su_port_wait_events)(su_port_t *port, su_duration_t timeout);
  int (*su_port_getmsgs)(su_port_t *port);
  /* Extension from >= 1.12.5 */
  int (*su_port_getmsgs_from)(su_port_t *port, su_port_t *cloneport);
  char const *(*su_port_name)(su_port_t const *port);
  int (*su_port_start_shared)(su_root_t *root,
			      su_clone_r return_clone,
			      su_root_magic_t *magic,
			      su_root_init_f init,
			      su_root_deinit_f deinit);
  void (*su_port_wait)(su_clone_r rclone);
  int (*su_port_execute)(su_task_r const task,
			 int (*function)(void *), void *arg,
			 int *return_value);

  /* >= 1.12.11 */
  su_timer_queue_t *(*su_port_deferrable)(su_port_t *port);
  int (*su_port_max_defer)(su_port_t *port,
			   su_duration_t *return_duration,
			   su_duration_t *set_duration);
  int (*su_port_wakeup)(su_port_t *port);
  int (*su_port_is_running)(su_port_t const *port);
} su_port_vtable_t;

SOFIAPUBFUN su_port_t *su_port_create(void)
     __attribute__((__malloc__));

/* Extension from >= 1.12.5 */

SOFIAPUBFUN void su_msg_delivery_report(su_msg_r msg);
SOFIAPUBFUN su_duration_t su_timer_next_expires(su_timer_queue_t const *timers,
						su_time_t now);
SOFIAPUBFUN su_root_t *su_root_create_with_port(su_root_magic_t *magic,
						su_port_t *port)
  __attribute__((__malloc__));

/* Extension from >= 1.12.6 */

SOFIAPUBFUN char const *su_port_name(su_port_t const *port);

SOFIAPUBFUN int su_timer_reset_all(su_timer_queue_t *, su_task_r );

/* ---------------------------------------------------------------------- */

/* React to multiple events per one poll() to make sure
 * that high-priority events can never completely mask other events.
 * Enabled by default on all platforms except WIN32 */
#if !defined(WIN32)
#define SU_ENABLE_MULTISHOT_POLL 1
#else
#define SU_ENABLE_MULTISHOT_POLL 0
#endif

/* ---------------------------------------------------------------------- */
/* Virtual functions */

typedef struct su_virtual_port_s {
  su_home_t        sup_home[1];
  su_port_vtable_t const *sup_vtable;
} su_virtual_port_t;

su_inline
su_home_t *su_port_home(su_port_t const *self)
{
  return (su_home_t *)self;
}

su_inline
void su_port_lock(su_port_t *self, char const *who)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  base->sup_vtable->su_port_lock(self, who);
}

su_inline
void su_port_unlock(su_port_t *self, char const *who)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  base->sup_vtable->su_port_unlock(self, who);
}

su_inline
void su_port_incref(su_port_t *self, char const *who)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  base->sup_vtable->su_port_incref(self, who);
}

su_inline
void su_port_decref(su_port_t *self, char const *who)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  base->sup_vtable->su_port_decref(self, 0, who);
}

su_inline
void su_port_zapref(su_port_t *self, char const *who)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  base->sup_vtable->su_port_decref(self, 1, who);
}

su_inline
struct _GSource *su_port_gsource(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_gsource(self);
}

su_inline
int su_port_send(su_port_t *self, su_msg_r rmsg)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_send(self, rmsg);
}

su_inline
int su_port_wakeup(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_wakeup(self);
}

su_inline
int su_port_register(su_port_t *self,
		     su_root_t *root,
		     su_wait_t *wait,
		     su_wakeup_f callback,
		     su_wakeup_arg_t *arg,
		     int priority)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->
    su_port_register(self, root, wait, callback, arg, priority);
}

su_inline
int su_port_unregister(su_port_t *self,
		       su_root_t *root,
		       su_wait_t *wait,
		       su_wakeup_f callback,
		       su_wakeup_arg_t *arg)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->
    su_port_unregister(self, root, wait, callback, arg);
}

su_inline
int su_port_deregister(su_port_t *self, int i)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->
    su_port_deregister(self, i);
}

su_inline
int su_port_unregister_all(su_port_t *self,
			   su_root_t *root)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->
    su_port_unregister_all(self, root);
}

su_inline
int su_port_eventmask(su_port_t *self, int index, int socket, int events)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->
    su_port_eventmask(self, index, socket, events);
}

su_inline
int su_port_wait_events(su_port_t *self, su_duration_t timeout)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  if (base->sup_vtable->su_port_wait_events == NULL)
    return errno = ENOSYS, -1;
  return base->sup_vtable->
    su_port_wait_events(self, timeout);
}

su_inline
void su_port_run(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  base->sup_vtable->su_port_run(self);
}

su_inline
void su_port_break(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  base->sup_vtable->su_port_break(self);
}

su_inline
su_duration_t su_port_step(su_port_t *self, su_duration_t tout)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_step(self, tout);
}


su_inline
int su_port_own_thread(su_port_t const *self)
{
  su_virtual_port_t const *base = (su_virtual_port_t *)self;
  return base->sup_vtable->
    su_port_thread((su_port_t *)self, su_port_thread_op_is_obtained) == 2;
}

su_inline int su_port_has_thread(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_thread(self, su_port_thread_op_is_obtained);
}

su_inline int su_port_release(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_thread(self, su_port_thread_op_release);
}

su_inline int su_port_obtain(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_thread(self, su_port_thread_op_obtain);
}

su_inline
int su_port_add_prepoll(su_port_t *self,
			su_root_t *root,
			su_prepoll_f *prepoll,
			su_prepoll_magic_t *magic)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_add_prepoll(self, root, prepoll, magic);
}

su_inline
int su_port_remove_prepoll(su_port_t *self,
			   su_root_t *root)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_remove_prepoll(self, root);
}

su_inline
su_timer_queue_t *su_port_timers(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_timers(self);
}

su_inline
int su_port_multishot(su_port_t *self, int multishot)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_multishot(self, multishot);
}

su_inline
int su_port_getmsgs(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_getmsgs(self);
}

su_inline
int su_port_getmsgs_from(su_port_t *self, su_port_t *cloneport)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base->sup_vtable->su_port_getmsgs_from(self, cloneport);
}

/** Extension from >= 1.12.11 */

su_inline
su_timer_queue_t *su_port_deferrable(su_port_t *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;

  if (base == NULL) {
    errno = EFAULT;
    return NULL;
  }

  return base->sup_vtable->su_port_deferrable(self);
}

su_inline
int su_port_max_defer(su_port_t *self,
		      su_duration_t *return_duration,
		      su_duration_t *set_duration)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;

  if (base == NULL)
    return (errno = EFAULT), -1;

  return base->sup_vtable->su_port_max_defer(self,
					     return_duration,
					     set_duration);
}

su_inline
int su_port_is_running(su_port_t const *self)
{
  su_virtual_port_t *base = (su_virtual_port_t *)self;
  return base && base->sup_vtable->su_port_is_running(self);
}

SOFIAPUBFUN void su_port_wait(su_clone_r rclone);

SOFIAPUBFUN int su_port_execute(su_task_r const task,
				int (*function)(void *), void *arg,
				int *return_value);

/* ---------------------------------------------------------------------- */

/**@internal Base port object.
 *
 * Port is a per-thread reactor. Multiple root objects executed by a single
 * thread share the su_port_t object.
 */
typedef struct su_base_port_s {
  su_home_t        sup_home[1];
  su_port_vtable_t const *sup_vtable;

  /* Implementation may vary stuff below, too. */

  /* Pre-poll callback */
  su_prepoll_f    *sup_prepoll;
  su_prepoll_magic_t *sup_pp_magic;
  su_root_t       *sup_pp_root;

  /* Message list - this is protected by su_port_lock()/su_port_unlock() */
  su_msg_t        *sup_head, **sup_tail;

  /* Timer list */
  su_timer_queue_t sup_timers, sup_deferrable;

  su_duration_t    sup_max_defer; /**< Maximum time to defer */

  unsigned         sup_running;	  /**< In su_root_run() loop? */
} su_base_port_t;

/* Base methods */

SOFIAPUBFUN int su_base_port_init(su_port_t *, su_port_vtable_t const *);
SOFIAPUBFUN void su_base_port_deinit(su_port_t *self);

SOFIAPUBFUN void su_base_port_lock(su_port_t *self, char const *who);
SOFIAPUBFUN void su_base_port_unlock(su_port_t *self, char const *who);

SOFIAPUBFUN int su_base_port_thread(su_port_t const *self,
				    enum su_port_thread_op op);

SOFIAPUBFUN void su_base_port_incref(su_port_t *self, char const *who);
SOFIAPUBFUN int su_base_port_decref(su_port_t *self,
				    int blocking,
				    char const *who);

SOFIAPUBFUN struct _GSource *su_base_port_gsource(su_port_t *self);

SOFIAPUBFUN su_socket_t su_base_port_mbox(su_port_t *self);
SOFIAPUBFUN int su_base_port_send(su_port_t *self, su_msg_r rmsg);
SOFIAPUBFUN int su_base_port_getmsgs(su_port_t *self);
SOFIAPUBFUN int su_base_port_getmsgs_from(su_port_t *self,
					   su_port_t *from);

SOFIAPUBFUN void su_base_port_run(su_port_t *self);
SOFIAPUBFUN void su_base_port_break(su_port_t *self);
SOFIAPUBFUN su_duration_t su_base_port_step(su_port_t *self,
					    su_duration_t tout);

SOFIAPUBFUN int su_base_port_add_prepoll(su_port_t *self,
					 su_root_t *root,
					 su_prepoll_f *,
					 su_prepoll_magic_t *);

SOFIAPUBFUN int su_base_port_remove_prepoll(su_port_t *self, su_root_t *root);

SOFIAPUBFUN su_timer_queue_t *su_base_port_timers(su_port_t *self);

SOFIAPUBFUN int su_base_port_multishot(su_port_t *self, int multishot);

SOFIAPUBFUN int su_base_port_start_shared(su_root_t *parent,
					  su_clone_r return_clone,
					  su_root_magic_t *magic,
					  su_root_init_f init,
					  su_root_deinit_f deinit);
SOFIAPUBFUN void su_base_port_wait(su_clone_r rclone);

SOFIAPUBFUN su_timer_queue_t *su_base_port_deferrable(su_port_t *self);

SOFIAPUBFUN int su_base_port_max_defer(su_port_t *self,
				       su_duration_t *return_duration,
				       su_duration_t *set_duration);

SOFIAPUBFUN int su_base_port_is_running(su_port_t const *self);

/* ---------------------------------------------------------------------- */

#if SU_HAVE_PTHREADS

#include <pthread.h>

/** @internal Pthread port object */
typedef struct su_pthread_port_s {
  su_base_port_t   sup_base[1];
  struct su_pthread_port_waiting_parent
                  *sup_waiting_parent;
  pthread_t        sup_tid;
  pthread_mutex_t  sup_obtained[1];

#if 0 				/* Pausing and resuming are not used */
  pthread_mutex_t  sup_runlock[1];
  pthread_cond_t   sup_resume[1];
  short            sup_paused;	/**< True if thread is paused */
#endif
  short            sup_thread;	/**< True if thread is active */
} su_pthread_port_t;

/* Pthread methods */

SOFIAPUBFUN int su_pthread_port_init(su_port_t *, su_port_vtable_t const *);
SOFIAPUBFUN void su_pthread_port_deinit(su_port_t *self);

SOFIAPUBFUN void su_pthread_port_lock(su_port_t *self, char const *who);
SOFIAPUBFUN void su_pthread_port_unlock(su_port_t *self, char const *who);

SOFIAPUBFUN int su_pthread_port_thread(su_port_t *self,
				       enum su_port_thread_op op);

#if 0				/* not yet  */
SOFIAPUBFUN int su_pthread_port_send(su_port_t *self, su_msg_r rmsg);

SOFIAPUBFUN su_port_t *su_pthread_port_create(void);
SOFIAPUBFUN su_port_t *su_pthread_port_start(su_root_t *parent,
					     su_clone_r return_clone,
					     su_root_magic_t *magic,
					     su_root_init_f init,
					     su_root_deinit_f deinit);
#endif

SOFIAPUBFUN int su_pthreaded_port_start(su_port_create_f *create,
					su_root_t *parent,
					su_clone_r return_clone,
					su_root_magic_t *magic,
					su_root_init_f init,
					su_root_deinit_f deinit);

SOFIAPUBFUN void su_pthread_port_wait(su_clone_r rclone);
SOFIAPUBFUN int su_pthread_port_execute(su_task_r const task,
					int (*function)(void *), void *arg,
					int *return_value);

#if 0
SOFIAPUBFUN int su_pthread_port_pause(su_port_t *self);
SOFIAPUBFUN int su_pthread_port_resume(su_port_t *self);
#endif

#else

typedef su_base_port_t su_pthread_port_t;

#define su_pthread_port_init   su_base_port_init
#define su_pthread_port_deinit su_base_port_deinit
#define su_pthread_port_lock   su_base_port_lock
#define su_pthread_port_unlock su_base_port_unlock
#define su_pthread_port_thread su_base_port_thread
#define su_pthread_port_wait   su_base_port_wait
#define su_pthread_port_execute  su_base_port_execute

#endif

/* ====================================================================== */
/* Mailbox port using sockets */

#define SU_MBOX_SIZE 2

typedef struct su_socket_port_s {
  su_pthread_port_t sup_base[1];
  int               sup_mbox_index;
  su_socket_t       sup_mbox[SU_MBOX_SIZE];
} su_socket_port_t;

SOFIAPUBFUN int su_socket_port_init(su_socket_port_t *,
				    su_port_vtable_t const *);
SOFIAPUBFUN void su_socket_port_deinit(su_socket_port_t *self);
SOFIAPUBFUN int su_socket_port_send(su_port_t *self, su_msg_r rmsg);
SOFIAPUBFUN int su_socket_port_wakeup(su_port_t *self);

SOFIA_END_DECLS

#endif /* SU_PORT_H */
