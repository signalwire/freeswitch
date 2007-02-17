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

/**@IFILE su_port.h 
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

/** Message */
struct su_msg_s {
  isize_t        sum_size;
  su_msg_t      *sum_next;
  su_task_r      sum_to;
  su_task_r      sum_from;
  su_msg_f       sum_func;
  su_msg_f       sum_report;
  su_msg_arg_t   sum_data[1];		/* minimum size, may be extended */
};

struct _GSource;

/** Root structure */
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

/** Virtual function table for port */
typedef struct {
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
  
  int (*su_port_own_thread)(su_port_t const *port);
  
  int (*su_port_add_prepoll)(su_port_t *port,
			     su_root_t *root, 
			     su_prepoll_f *, 
			     su_prepoll_magic_t *);
  
  int (*su_port_remove_prepoll)(su_port_t *port,
				su_root_t *root);

  su_timer_t **(*su_port_timers)(su_port_t *port);

  int (*su_port_multishot)(su_port_t *port, int multishot);

  int (*su_port_threadsafe)(su_port_t *port);
  
  /* Extension from > 1.12.0 */
  int (*su_port_yield)(su_port_t *port);
} su_port_vtable_t;

SOFIAPUBFUN su_port_t *su_port_create(void)
     __attribute__((__malloc__));

SOFIAPUBFUN void su_msg_delivery_report(su_msg_r msg);
SOFIAPUBFUN su_duration_t su_timer_next_expires(su_timer_t const * t,
						su_time_t now);
SOFIAPUBFUN su_root_t *su_root_create_with_port(su_root_magic_t *magic,
						su_port_t *port);

#if SU_PORT_IMPLEMENTATION

#else
struct su_port_s {
  su_home_t               sup_home[1];
  su_port_vtable_t const *sup_vtable;
};

static inline
void su_port_lock(su_port_t *self, char const *who)
{
  if (self) self->sup_vtable->su_port_lock(self, who);
}

#define SU_PORT_LOCK(p, f)      (su_port_lock(p, #f))
 
static inline
void su_port_unlock(su_port_t *self, char const *who)
{
  if (self) self->sup_vtable->su_port_unlock(self, who);
}

#define SU_PORT_UNLOCK(p, f)    (su_port_unlock(p, #f))

static inline
void su_port_incref(su_port_t *self, char const *who)
{
  if (self) self->sup_vtable->su_port_incref(self, who);
}

#define SU_PORT_INCREF(p, f)    (su_port_incref(p, #f))
 
static inline
void su_port_decref(su_port_t *self, char const *who)
{
  if (self) self->sup_vtable->su_port_decref(self, 0, who);
}

#define SU_PORT_DECREF(p, f)    (su_port_decref(p, #f))

static inline
void su_port_zapref(su_port_t *self, char const *who)
{
  if (self) self->sup_vtable->su_port_decref(self, 1, who);
}

#define SU_PORT_ZAPREF(p, f)    (su_port_zapref(p, #f))

static inline
struct _GSource *su_port_gsource(su_port_t *self)
{
  return self ? self->sup_vtable->su_port_gsource(self) : NULL;
}


static inline
int su_port_send(su_port_t *self, su_msg_r rmsg)
{
  if (self) 
    return self->sup_vtable->su_port_send(self, rmsg);
  errno = EINVAL;
  return -1;
}


static inline
int su_port_register(su_port_t *self,
		     su_root_t *root, 
		     su_wait_t *wait, 
		     su_wakeup_f callback,
		     su_wakeup_arg_t *arg,
		     int priority)
{
  if (self)
    return self->sup_vtable->su_port_register(self, root, wait,
					      callback, arg, priority);
  errno = EINVAL;
  return -1;
}

static inline
int su_port_unregister(su_port_t *self,
		       su_root_t *root, 
		       su_wait_t *wait,	
		       su_wakeup_f callback, 
		       su_wakeup_arg_t *arg)
{
  if (self)
    return self->sup_vtable->
      su_port_unregister(self, root, wait, callback, arg);
  errno = EINVAL;
  return -1;
}

static inline
int su_port_deregister(su_port_t *self, int i)
{
  if (self)
    return self->sup_vtable->
      su_port_deregister(self, i);
  errno = EINVAL;
  return -1;
}

static inline
int su_port_unregister_all(su_port_t *self,
			   su_root_t *root)
{
  if (self)
    return self->sup_vtable->
      su_port_unregister_all(self, root);
  errno = EINVAL;
  return -1;
}

static inline
int su_port_eventmask(su_port_t *self, int index, int socket, int events)
{
  if (self)
    return self->sup_vtable->
      su_port_eventmask(self, index, socket, events);
  assert(self);
  errno = EINVAL;
  return -1;
}

static inline
void su_port_run(su_port_t *self)
{
  if (self)
    self->sup_vtable->su_port_run(self);
}

static inline
void su_port_break(su_port_t *self)
{
  if (self)
    self->sup_vtable->su_port_break(self);
}

static inline
su_duration_t su_port_step(su_port_t *self, su_duration_t tout)
{
  if (self)
    return self->sup_vtable->su_port_step(self, tout);
  errno = EINVAL;
  return (su_duration_t)-1;
}


static inline
int su_port_own_thread(su_port_t const *self)
{
  return self == NULL || self->sup_vtable->su_port_own_thread(self);
}

static inline
int su_port_add_prepoll(su_port_t *self,
			su_root_t *root, 
			su_prepoll_f *prepoll, 
			su_prepoll_magic_t *magic)
{
  if (self)
    return self->sup_vtable->
      su_port_add_prepoll(self, root, prepoll, magic);
  errno = EINVAL;
  return -1;
}

static inline
int su_port_remove_prepoll(su_port_t *self,
			   su_root_t *root)
{
  if (self)
    return self->sup_vtable->su_port_remove_prepoll(self, root);
  errno = EINVAL;
  return -1;
}

static inline
su_timer_t **su_port_timers(su_port_t *self)
{
  if (self)
    return self->sup_vtable->su_port_timers(self);
  errno = EINVAL;
  return NULL;
}

static inline
int su_port_multishot(su_port_t *self, int multishot)
{
  if (self)
    return self->sup_vtable->su_port_multishot(self, multishot);

  assert(self);
  errno = EINVAL;
  return -1;
}

static inline
int su_port_threadsafe(su_port_t *self)
{
  if (self)
    return self->sup_vtable->su_port_threadsafe(self);

  assert(self);
  errno = EINVAL;
  return -1;
}


#endif

SOFIA_END_DECLS

#endif /* SU_PORT_H */
