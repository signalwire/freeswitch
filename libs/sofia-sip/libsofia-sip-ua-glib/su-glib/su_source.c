/*
 * This file is part of the Sofia-SIP package
 *
 * Copyright (C) 2005-2009 Nokia Corporation.
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

/**
 * @file su_source.c
 * @brief Wrapper for glib GSource.
 *
 * Refs:
 *  - http://sofia-sip.sourceforge.net/refdocs/su/group__su__wait.html
 *  - http://developer.gnome.org/doc/API/glib/glib-the-main-event-loop.html
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>.
 *
 * @date Created: Thu Mar  4 15:15:15 2004 ppessi
 *
 */

#include "config.h"

#ifdef SYMBIAN
#include <e32def.h>
#endif

#ifndef __GLIB_H__
#include <glib.h>
#endif

#if HAVE_OPEN_C
#include <glib/gthread.h>
#include <glib_global.h>
#endif

#define SU_PORT_IMPLEMENTATION 1

#define SU_MSG_ARG_T union { char anoymous[4]; }

#define su_port_s su_source_s

#include "sofia-sip/su_source.h"
#include "sofia-sip/su_glib.h"

#include "sofia-sip/su.h"
#include "su_port.h"
#include "sofia-sip/su_alloc.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

#if 1
#define PORT_LOCK_DEBUG(x)  ((void)0)
#else
#define PORT_LOCK_DEBUG(x)  printf x
#endif

static su_port_t *su_source_port_create(void) __attribute__((__malloc__));
static gboolean su_source_prepare(GSource *gs, gint *return_tout);
static gboolean su_source_check(GSource *gs);
static gboolean su_source_dispatch(GSource *gs,
				   GSourceFunc callback,
				   gpointer user_data);
static void su_source_finalize(GSource *source);

static
GSourceFuncs su_source_funcs[1] = {{
    su_source_prepare,
    su_source_check,
    su_source_dispatch,
    su_source_finalize,
    NULL,
    NULL
  }};

static int su_source_port_init(su_port_t *self, su_port_vtable_t const *vtable);
static void su_source_port_deinit(su_port_t *self);

static void su_source_lock(su_port_t *self, char const *who);
static void su_source_unlock(su_port_t *self, char const *who);
static void su_source_incref(su_port_t *self, char const *who);
static void su_source_decref(su_port_t *self, int blocking, char const *who);
static struct _GSource *su_source_gsource(su_port_t *port);

static int su_source_register(su_port_t *self,
			    su_root_t *root,
			    su_wait_t *wait,
			    su_wakeup_f callback,
			    su_wakeup_arg_t *arg,
			    int priority);
static int su_source_unregister(su_port_t *port,
			      su_root_t *root,
			      su_wait_t *wait,
			      su_wakeup_f callback,
			      su_wakeup_arg_t *arg);
static int su_source_deregister(su_port_t *self, int i);
static int su_source_unregister_all(su_port_t *self,
				  su_root_t *root);
static int su_source_eventmask(su_port_t *self,
			     int index, int socket, int events);
static void su_source_run(su_port_t *self);
static void su_source_break(su_port_t *self);
static su_duration_t su_source_step(su_port_t *self, su_duration_t tout);
static int su_source_thread(su_port_t *self, enum su_port_thread_op op);
static int su_source_add_prepoll(su_port_t *port,
				 su_root_t *root,
				 su_prepoll_f *,
				 su_prepoll_magic_t *);
static int su_source_remove_prepoll(su_port_t *port,
				  su_root_t *root);
static int su_source_multishot(su_port_t *self, int multishot);
static int su_source_wakeup(su_port_t *self);
static int su_source_is_running(su_port_t const *self);

static char const *su_source_name(su_port_t const *self);

static
su_port_vtable_t const su_source_port_vtable[1] =
  {{
      /* su_vtable_size: */ sizeof su_source_port_vtable,
      su_source_lock,
      su_source_unlock,

      su_source_incref,
      su_source_decref,

      su_source_gsource,

      su_base_port_send,
      su_source_register,
      su_source_unregister,
      su_source_deregister,
      su_source_unregister_all,
      su_source_eventmask,
      su_source_run,
      su_source_break,
      su_source_step,
      su_source_thread,
      su_source_add_prepoll,
      su_source_remove_prepoll,
      su_base_port_timers,
      su_source_multishot,
      /*su_source_wait_events*/ NULL,
      su_base_port_getmsgs,
      su_base_port_getmsgs_from,
      su_source_name,
      su_base_port_start_shared,
      su_base_port_wait,
      NULL,
      su_base_port_deferrable,
      su_base_port_max_defer,
      su_source_wakeup,
      su_source_is_running,
    }};

static char const *su_source_name(su_port_t const *self)
{
  return "GSource";
}

/**
 * Port is a per-thread reactor.
 *
 * Multiple root objects executed by single thread share a su_port_t object.
 */
struct su_source_s {
  su_base_port_t   sup_base[1];

  GThread         *sup_tid;
  GStaticMutex     sup_obtained[1];

  GStaticMutex     sup_mutex[1];

  GSource         *sup_source;	/**< Backpointer to source */
  GMainLoop       *sup_main_loop; /**< Reference to mainloop while running */

  /* Waits */
  unsigned         sup_registers; /** Counter incremented by
				      su_port_register() or
				      su_port_unregister()
				  */
  unsigned         sup_n_waits;
  unsigned         sup_size_waits;
  unsigned         sup_max_index;
  unsigned        *sup_indices;
  su_wait_t       *sup_waits;
  su_wakeup_f     *sup_wait_cbs;
  su_wakeup_arg_t**sup_wait_args;
  su_root_t      **sup_wait_roots;
};

typedef struct _SuSource
{
  GSource    ss_source[1];
  su_port_t  ss_port[1];
} SuSource;

#define SU_SOURCE_OWN_THREAD(p)   ((p)->sup_tid == g_thread_self())

#if 1
#define SU_SOURCE_INCREF(p, f)    (g_source_ref(p->sup_source))
#define SU_SOURCE_DECREF(p, f)    (g_source_unref(p->sup_source))

#else

/* Debugging versions */
#define SU_SOURCE_INCREF(p, f)    (g_source_ref(p->sup_source), printf("incref(%p) by %s\n", (p), f))
#define SU_SOURCE_DECREF(p, f)    do { printf("decref(%p) by %s\n", (p), f), \
  g_source_unref(p->sup_source); } while(0)

#endif

#if HAVE_FUNC
#define enter (void)SU_DEBUG_9(("%s: entering\n", __func__))
#elif HAVE_FUNCTION
#define enter (void)SU_DEBUG_9(("%s: entering\n", __FUNCTION__))
#else
#define enter (void)0
#endif

/*=============== Public function definitions ===============*/

/** Create a root that uses GSource as reactor */
su_root_t *su_glib_root_create(su_root_magic_t *magic)
{
  return su_root_create_with_port(magic, su_source_port_create());
}

/** Deprecated */
su_root_t *su_root_source_create(su_root_magic_t *magic)
{
  return su_glib_root_create(magic);
}

/**
 * Returns a GSource object for the root
 *
 * Note that you need to unref the GSource with g_source_unref()
 * before destroying the root object.
 *
 * @return NULL on error (for instance if root was not created with
 *         su_glib_root_create())
 */
GSource *su_glib_root_gsource(su_root_t *root)
{
  g_assert(root);
  return su_root_gsource(root);
}

/*=============== Private function definitions ===============*/

/** Initialize source port */
static int su_source_port_init(su_port_t *self,
			       su_port_vtable_t const *vtable)
{
  GSource *gs = (GSource *)((char *)self - offsetof(SuSource, ss_port));

  self->sup_source = gs;

  g_static_mutex_init(self->sup_obtained);

  g_static_mutex_init(self->sup_mutex);

  return su_base_port_init(self, vtable);
}


static void su_source_port_deinit(su_port_t *self)
{
  su_base_port_deinit(self);

  g_static_mutex_free(self->sup_mutex);
  g_static_mutex_free(self->sup_obtained);

  if (self->sup_indices)
    free (self->sup_indices), self->sup_indices = NULL;
  if (self->sup_waits)
    free (self->sup_waits), self->sup_waits = NULL;
  if (self->sup_wait_cbs)
    free (self->sup_wait_cbs), self->sup_wait_cbs = NULL;
  if (self->sup_wait_args)
    free (self->sup_wait_args), self->sup_wait_args = NULL;
  if (self->sup_wait_roots)
    free (self->sup_wait_roots), self->sup_wait_roots = NULL;

  su_home_deinit(self->sup_base->sup_home);
}


/** @internal Destroy a port. */
static
void su_source_finalize(GSource *gs)
{
  SuSource *ss = (SuSource *)gs;
  assert(gs);
  SU_DEBUG_9(("su_source_finalize() called\n"));
  su_source_port_deinit(ss->ss_port);
}

/** @internal Send a message to the port. */
int su_source_wakeup(su_port_t *self)
{
  GMainContext *gmc = g_source_get_context(self->sup_source);

  if (gmc)
    g_main_context_wakeup(gmc);

  return 0;
}

/** @internal
 *
 * Change or query ownership of the port object.
 *
 * @param self pointer to a port object
 * @param op operation
 *
 * @ERRORS
 * @ERROR EALREADY port already has an owner (or has no owner)
 */
static int su_source_thread(su_port_t *self, enum su_port_thread_op op)
{
  GThread *me = g_thread_self();

  switch (op) {

  case su_port_thread_op_is_obtained:
    if (self->sup_tid == me)
      return 2;
    else if (self->sup_tid)
      return 1;
    else
      return 0;

  case su_port_thread_op_release:
    if (self->sup_tid != me)
      return errno = EALREADY, -1;
    self->sup_tid = NULL;
    g_static_mutex_unlock(self->sup_obtained);
    return 0;

  case su_port_thread_op_obtain:
    if (su_home_threadsafe(su_port_home(self)) == -1)
      return -1;
    g_static_mutex_lock(self->sup_obtained);
    self->sup_tid = me;
    return 0;

  default:
    return errno = ENOSYS, -1;
  }
}

/* -- Registering and unregistering ------------------------------------- */

/* Seconds from 1.1.1900 to 1.1.1970 */
#define NTP_EPOCH 2208988800UL

/** Prepare to wait - calculate time to next timer */
static
gboolean su_source_prepare(GSource *gs, gint *return_tout)
{
  SuSource *ss = (SuSource *)gs;
  su_port_t *self = ss->ss_port;
  su_duration_t tout = SU_WAIT_FOREVER;

  enter;

  if (self->sup_base->sup_head) {
    *return_tout = 0;
    return TRUE;
  }

  if (self->sup_base->sup_timers || self->sup_base->sup_deferrable) {
    su_time_t now;
    GTimeVal  gtimeval;

    g_source_get_current_time(gs, &gtimeval);
    now.tv_sec = gtimeval.tv_sec + 2208988800UL;
    now.tv_usec = gtimeval.tv_usec;

    tout = su_timer_next_expires(&self->sup_base->sup_timers, now);

    if (self->sup_base->sup_deferrable) {
      su_duration_t tout_defer;

      tout_defer = su_timer_next_expires(&self->sup_base->sup_deferrable, now);

      if (tout_defer < self->sup_base->sup_max_defer)
        tout_defer = self->sup_base->sup_max_defer;

      if (tout > tout_defer)
        tout = tout_defer;
    }
  }

  *return_tout = (tout >= 0 && tout <= (su_duration_t)G_MAXINT)?
      (gint)tout : -1;

  return (tout == 0);
}

static
gboolean su_source_check(GSource *gs)
{
  SuSource *ss = (SuSource *)gs;
  su_port_t *self = ss->ss_port;
  gint tout;
#if SU_HAVE_POLL
  unsigned i, I;
#endif

  enter;

#if SU_HAVE_POLL
  I = self->sup_n_waits;

  for (i = 0; i < I; i++) {
    if (self->sup_waits[i].revents)
      return TRUE;
  }
#endif

  return su_source_prepare(gs, &tout);
}

static
gboolean su_source_dispatch(GSource *gs,
			    GSourceFunc callback,
			    gpointer user_data)
{
  SuSource *ss = (SuSource *)gs;
  su_port_t *self = ss->ss_port;

  enter;

  if (self->sup_base->sup_head)
    su_base_port_getmsgs(self);

  if (self->sup_base->sup_timers || self->sup_base->sup_deferrable) {
    su_time_t now;
    GTimeVal  gtimeval;
    su_duration_t tout;

    tout = SU_DURATION_MAX;

    g_source_get_current_time(gs, &gtimeval);

    now.tv_sec = gtimeval.tv_sec + 2208988800UL;
    now.tv_usec = gtimeval.tv_usec;

    su_timer_expire(&self->sup_base->sup_timers, &tout, now);
    su_timer_expire(&self->sup_base->sup_deferrable, &tout, now);
  }

#if SU_HAVE_POLL
  {
    su_root_t *root;
    su_wait_t *waits = self->sup_waits;
    unsigned i, n = self->sup_n_waits;
    unsigned version = self->sup_registers;

    for (i = 0; i < n; i++) {
      if (waits[i].revents) {
	root = self->sup_wait_roots[i];
	self->sup_wait_cbs[i](root ? su_root_magic(root) : NULL,
			      &waits[i],
			      self->sup_wait_args[i]);
	/* Callback used su_register()/su_unregister() */
	if (version != self->sup_registers)
	  break;
      }
    }
  }
#endif

  if (!callback)
    return TRUE;

  return callback(user_data);
}

static void su_source_lock(su_port_t *self, char const *who)
{
  PORT_LOCK_DEBUG(("%p at %s locking(%p)...",
		   (void *)g_thread_self(), who, self));
  g_static_mutex_lock(self->sup_mutex);

  PORT_LOCK_DEBUG((" ...%p at %s locked(%p)...",
		   (void *)g_thread_self(), who, self));
}

static void su_source_unlock(su_port_t *self, char const *who)
{
  g_static_mutex_unlock(self->sup_mutex);

  PORT_LOCK_DEBUG((" ...%p at %s unlocked(%p)\n",
		   (void *)g_thread_self(), who, self));
}

static void su_source_incref(su_port_t *self, char const *who)
{
  SU_SOURCE_INCREF(self, who);
}

static void su_source_decref(su_port_t *self, int blocking, char const *who)
{
  /* XXX - blocking? */
  SU_SOURCE_DECREF(self, who);
}

GSource *su_source_gsource(su_port_t *self)
{
  return self->sup_source;
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
 *   The function @su_source_register returns nonzero index of the wait object,
 *   or -1 upon an error.  */
int su_source_register(su_port_t *self,
		       su_root_t *root,
		       su_wait_t *wait,
		       su_wakeup_f callback,
		       su_wakeup_arg_t *arg,
		       int priority)
{
  unsigned i, j, I;
  unsigned n;

  enter;

  assert(SU_SOURCE_OWN_THREAD(self));

  n = self->sup_n_waits;

  if (n >= self->sup_size_waits) {
    /* Reallocate size arrays */
    unsigned size;
    unsigned *indices;
    su_wait_t *waits;
    su_wakeup_f *wait_cbs;
    su_wakeup_arg_t **wait_args;
    su_root_t **wait_tasks;

    if (self->sup_size_waits == 0)
      size = SU_WAIT_MIN;
    else
      size = 2 * self->sup_size_waits;

    indices = realloc(self->sup_indices, size * sizeof(*indices));
    if (indices) {
      self->sup_indices = indices;

      for (i = self->sup_size_waits; i < size; i++)
	indices[i] = UINT_MAX;
    }

    for (i = 0; i < self->sup_n_waits; i++)
      g_source_remove_poll(self->sup_source, (GPollFD*)&self->sup_waits[i]);

    waits = realloc(self->sup_waits, size * sizeof(*waits));
    if (waits)
      self->sup_waits = waits;

    for (i = 0; i < self->sup_n_waits; i++)
      g_source_add_poll(self->sup_source, (GPollFD*)&waits[i]);

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

    if (!(indices && waits && wait_cbs && wait_args && wait_tasks)) {
      return -1;
    }

    self->sup_size_waits = size;
  }

  self->sup_n_waits++;

  if (priority > 0) {
    /* Insert */
    for (; n > 0; n--) {
      g_source_remove_poll(self->sup_source, (GPollFD*)&self->sup_waits[n-1]);
      self->sup_waits[n] = self->sup_waits[n-1];
      g_source_add_poll(self->sup_source, (GPollFD*)&self->sup_waits[n]);
      self->sup_wait_cbs[n] = self->sup_wait_cbs[n-1];
      self->sup_wait_args[n] = self->sup_wait_args[n-1];
      self->sup_wait_roots[n] = self->sup_wait_roots[n-1];
    }
  }
  else {
    /* Append - no need to move anything */
  }

  self->sup_waits[n] = *wait;
  g_source_add_poll(self->sup_source, (GPollFD*)&self->sup_waits[n]);
  self->sup_wait_cbs[n] = callback;
  self->sup_wait_args[n] = arg;
  self->sup_wait_roots[n] = root;

  I = self->sup_max_index;

  for (i = 0; i < I; i++)
    if (self->sup_indices[i] == UINT_MAX)
      break;
    else if (self->sup_indices[i] >= n)
      self->sup_indices[i]++;

  if (i == I)
    self->sup_max_index++;

  if (n + 1 < self->sup_n_waits)
    for (j = i; j < I; j++)
      if (self->sup_indices[j] != UINT_MAX &&
	  self->sup_indices[j] >= n)
	self->sup_indices[j]++;

  self->sup_indices[i] = n;

  self->sup_registers++;

  return i + 1;			/* 0 is failure */
}

/** Unregister a su_wait_t object.
 *
 *  The function su_source_unregister() unregisters a su_wait_t object. The
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
int su_source_unregister(su_port_t *self,
			 su_root_t *root,
			 su_wait_t *wait,
			 su_wakeup_f callback, /* XXX - ignored */
			 su_wakeup_arg_t *arg)
{
  unsigned n, N;
  unsigned i, I, j, *indices;

  enter;

  assert(self);
  assert(SU_SOURCE_OWN_THREAD(self));

  i = (unsigned)-1;
  N = self->sup_n_waits;
  I = self->sup_max_index;
  indices = self->sup_indices;

  for (n = 0; n < N; n++) {
    if (SU_WAIT_CMP(wait[0], self->sup_waits[n]) != 0)
      continue;

    /* Found - delete it */
    if (indices[n] == n)
      i = n;
    else for (i = 0; i < I; i++)
      if (indices[i] == n)
	break;

    assert(i < I);

    indices[i] = UINT_MAX;

    g_source_remove_poll(self->sup_source, (GPollFD*)&self->sup_waits[n]);

    self->sup_n_waits = N = N - 1;

    if (n < N)
      for (j = 0; j < I; j++)
	if (self->sup_indices[j] != UINT_MAX &&
	    self->sup_indices[j] > n)
	  self->sup_indices[j]--;

    for (; n < N; n++) {
      g_source_remove_poll(self->sup_source, (GPollFD*)&self->sup_waits[n+1]);
      self->sup_waits[n] = self->sup_waits[n+1];
      g_source_add_poll(self->sup_source, (GPollFD*)&self->sup_waits[n]);
      self->sup_wait_cbs[n] = self->sup_wait_cbs[n+1];
      self->sup_wait_args[n] = self->sup_wait_args[n+1];
      self->sup_wait_roots[n] = self->sup_wait_roots[n+1];
    }

    i += 1;	/* 0 is failure */

    if (i == I)
      self->sup_max_index--;

    break;
  }

  self->sup_registers++;

  return (int)i;
}

/** Deregister a su_wait_t object.
 *
 *  The function su_source_deregister() deregisters a su_wait_t registrattion.
 *  The wait object, a callback function and a argument are removed from the
 *  port object.
 *
 * @param self     - pointer to port object
 * @param i        - registration index
 *
 * @return Index of the wait object, or -1 upon an error.
 */
int su_source_deregister(su_port_t *self, int i)
{
  unsigned j, n, N;
  unsigned I, *indices;
  su_wait_t wait[1];

  enter;

  assert(self);
  assert(SU_SOURCE_OWN_THREAD(self));

  if (i <= 0)
    return -1;

  N = self->sup_n_waits;
  I = self->sup_max_index;
  indices = self->sup_indices;

  assert((unsigned)i < I + 1);

  n = indices[i - 1];

  if (n == UINT_MAX)
    return -1;

  self->sup_n_waits = N = N - 1;

  wait[0] = self->sup_waits[n];

  g_source_remove_poll(self->sup_source, (GPollFD*)&self->sup_waits[n]);

  if (n < N)
    for (j = 0; j < I; j++)
      if (self->sup_indices[j] != UINT_MAX &&
	  self->sup_indices[j] > n)
	self->sup_indices[j]--;

  for (; n < N; n++) {
    g_source_remove_poll(self->sup_source, (GPollFD*)&self->sup_waits[n + 1]);
    self->sup_waits[n] = self->sup_waits[n+1];
    g_source_add_poll(self->sup_source, (GPollFD*)&self->sup_waits[n]);
    self->sup_wait_cbs[n] = self->sup_wait_cbs[n+1];
    self->sup_wait_args[n] = self->sup_wait_args[n+1];
    self->sup_wait_roots[n] = self->sup_wait_roots[n+1];
  }

  indices[i - 1] = UINT_MAX;

  if ((unsigned)i == I)
    self->sup_max_index--;

  su_wait_destroy(wait);

  self->sup_registers++;

  return (int)i;
}

/** @internal
 * Unregister all su_wait_t objects.
 *
 * The function su_source_unregister_all() unregisters all su_wait_t objects
 * associated with given root object destroys all queued timers.
 *
 * @param  self     - pointer to port object
 * @param  root     - pointer to root object
 *
 * @return Number of wait objects removed.
 */
int su_source_unregister_all(su_port_t *self,
			     su_root_t *root)
{
  unsigned i, j;
  unsigned         n_waits;
  su_wait_t       *waits;
  su_wakeup_f     *wait_cbs;
  su_wakeup_arg_t**wait_args;
  su_root_t      **wait_roots;

  enter;

  assert(SU_SOURCE_OWN_THREAD(self));

  n_waits    = self->sup_n_waits;
  waits      = self->sup_waits;
  wait_cbs   = self->sup_wait_cbs;
  wait_args  = self->sup_wait_args;
  wait_roots = self->sup_wait_roots;

  for (i = j = 0; (unsigned)i < n_waits; i++) {
    if (wait_roots[i] == root) {
      /* XXX - we should free all resources associated with this */
      g_source_remove_poll(self->sup_source, (GPollFD*)&waits[i]);
      continue;
    }
    if (i != j) {
      g_source_remove_poll(self->sup_source, (GPollFD*)&waits[i]);
      waits[j] = waits[i];
      wait_cbs[j] = wait_cbs[i];
      wait_args[j] = wait_args[i];
      wait_roots[j] = wait_roots[i];
      g_source_add_poll(self->sup_source, (GPollFD*)&waits[i]);
    }
    j++;
  }

  self->sup_n_waits = j;
  self->sup_registers++;

  return n_waits - j;
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
static
int su_source_eventmask(su_port_t *self, int index, int socket, int events)
{
  unsigned n;
  int retval;

  enter;

  assert(self);
  assert(SU_SOURCE_OWN_THREAD(self));
  assert(0 < index && (unsigned)index <= self->sup_max_index);

  if (index <= 0 || (unsigned)index > self->sup_max_index)
    return -1;

  n = self->sup_indices[index - 1];

  if (n == UINT_MAX)
    return -1;

  g_source_remove_poll(self->sup_source, (GPollFD*)&self->sup_waits[n]);

  retval = su_wait_mask(&self->sup_waits[n], socket, events);

  g_source_add_poll(self->sup_source, (GPollFD*)&self->sup_waits[n]);

  return retval;
}

static
int su_source_multishot(su_port_t *self, int multishot)
{
  if (multishot == -1)
    return 1;
  else if (multishot == 0 || multishot == 1)
    return 1;			/* Always enabled */
  else
    return (errno = EINVAL), -1;
}


/** @internal Run the main loop.
 *
 * The main loop runs until su_source_break() is called from a callback.
 *
 * @param self     pointer to port object
 * */
static
void su_source_run(su_port_t *self)
{
  GMainContext *gmc;
  GMainLoop *gml;

  enter;

  gmc = g_source_get_context(self->sup_source);
  if (gmc && g_main_context_acquire(gmc)) {
    gml = g_main_loop_new(gmc, TRUE);
    self->sup_main_loop = gml;
    g_main_loop_run(gml);
    g_main_loop_unref(gml);
    self->sup_main_loop = NULL;
    g_main_context_release(gmc);
  }
}

static int su_source_is_running(su_port_t const *self)
{
  return self->sup_main_loop && g_main_loop_is_running(self->sup_main_loop);
}

/** @internal
 * The function @c su_source_break() is used to terminate execution of @c
 * su_source_run(). It can be called from a callback function.
 *
 * @param self     pointer to port
 *
 */
static
void su_source_break(su_port_t *self)
{
  enter;

  if (self->sup_main_loop)
    g_main_loop_quit(self->sup_main_loop);
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
 * @Return
 *   Milliseconds to the next invocation of timer, or @c SU_WAIT_FOREVER if
 *   there are no active timers.
 */
su_duration_t su_source_step(su_port_t *self, su_duration_t tout)
{
  GMainContext *gmc;

  enter;

  gmc = g_source_get_context(self->sup_source);

  if (gmc && g_main_context_acquire(gmc)) {
    GPollFD *fds = NULL;
    gint fds_size = 0;
    gint fds_wait;
    gint priority = G_MAXINT;
    gint src_tout = -1;

    g_main_context_prepare(gmc, &priority);

    fds_wait = g_main_context_query(gmc, priority, &src_tout, NULL, 0);
    while (fds_wait > fds_size) {
      fds = g_alloca(fds_wait * sizeof(fds[0]));
      fds_size = fds_wait;
      fds_wait = g_main_context_query(gmc, priority, &src_tout, fds, fds_size);
    }

    if (src_tout >= 0 && tout > (su_duration_t)src_tout)
      tout = src_tout;

    su_wait((su_wait_t *)fds, fds_wait, tout);

    g_main_context_check(gmc, priority, fds, fds_wait);

    g_main_context_dispatch(gmc);

    g_main_context_release(gmc);
  }

  return 0;
}

static int su_source_add_prepoll(su_port_t *port,
				 su_root_t *root,
				 su_prepoll_f *prepoll,
				 su_prepoll_magic_t *magic)
{
  /* We could call prepoll in su_source_prepare()?? */
  return -1;
}

static int su_source_remove_prepoll(su_port_t *port,
				  su_root_t *root)
{
  return -1;
}

#if 0
/** @internal
 *  Prints out the contents of the port.
 *
 * @param self pointer to a port
 * @param f    pointer to a file (if @c NULL, uses @c stdout).
 */
void su_source_dump(su_port_t const *self, FILE *f)
{
  int i;
#define IS_WAIT_IN(x) (((x)->events & SU_WAIT_IN) ? "IN" : "")
#define IS_WAIT_OUT(x) (((x)->events & SU_WAIT_OUT) ? "OUT" : "")
#define IS_WAIT_ACCEPT(x) (((x)->events & SU_WAIT_ACCEPT) ? "ACCEPT" : "")

  if (f == NULL)
    f = stdout;

  fprintf(f, "su_port_t at %p:\n", self);
  fprintf(f, "\tport is%s running\n", self->sup_running ? "" : "not ");
  fprintf(f, "\tport tid %p\n", (void *)self->sup_tid);
  fprintf(f, "\t%d wait objects\n", self->sup_n_waits);
  for (i = 0; i < self->sup_n_waits; i++) {

  }
}

#endif

/**@internal
 *
 * Allocates and initializes a reactor and message port object.
 *
 * @return
 *   If successful a pointer to the new message port is returned, otherwise
 *   NULL is returned.
 */
static su_port_t *su_source_port_create(void)
{
  SuSource *ss;
  su_port_t *self = NULL;

  SU_DEBUG_9(("su_source_port_create() called\n"));

  ss = (SuSource *)g_source_new(su_source_funcs, (sizeof *ss));

  if (ss) {
    self = ss->ss_port;
    if (su_source_port_init(self, su_source_port_vtable) < 0)
      g_source_unref(ss->ss_source), self = NULL;
  } else {
    su_perror("su_source_port_create(): g_source_new");
  }

  SU_DEBUG_1(("su_source_port_create() returns %p\n", (void *)self));

  return self;
}

/* No su_source_port_start */

/** Use su_source implementation when su_root_create() is called.
 *
 * @NEW_1_12_5
 */
void su_glib_prefer_gsource(void)
{
  su_port_prefer(su_source_port_create, NULL);
}
