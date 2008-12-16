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

/**@CFILE su_timer.c
 *
 * Timer interface for su_root.
 *
 * @author Pekka Pessi <Pekka.Pessi@nokia.com>
 * Created: Fri Apr 28 15:45:41 2000 ppessi
 */

#include "config.h"

#include "su_port.h"
#include "sofia-sip/su.h"
#include "sofia-sip/su_wait.h"
#include "sofia-sip/su_alloc.h"
#include "sofia-sip/rbtree.h"

#include "su_module_debug.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

/**@ingroup su_wait
 *
 * @page su_timer_t Timer Objects
 *
 *  Timers are used to schedule some task to be executed at given time or
 *  after a default interval. The default interval is specified when the
 *  timer is created. We call timer activation "setting the timer", and
 *  deactivation "resetting the timer" (as in SDL). When the given time has
 *  arrived or the default interval has elapsed, the timer expires and
 *  it is ready for execution.
 *
 *  The functions used to create, destroy, activate, and manage timers are
 *  as follows:
 *   - su_timer_create(),
 *   - su_timer_destroy(),
 *   - su_timer_set_interval(),
 *   - su_timer_set_at(),
 *   - su_timer_set(),
 *   - su_timer_set_for_ever(),
 *   - su_timer_run(),
 *   - su_timer_reset(), and
 *   - su_timer_root().
 *
 * @note
 * Timers use poll() to wake up waiting thread. On Linux, the timer
 * granularity is determined by HZ kernel parameter, which decided when the
 * kernel was compiled. With kernel 2.4 the default granularity is 10
 * milliseconds, and minimum duration of a timer is approximately 20
 * milliseconds. Naturally, using RTC would give better timing results, but
 * RTC usage above 64 Hz is privileged operation.
 *
 * @par
 * On Windows, the granularity is determined by the real-time clock timer.
 * By default, it uses the 18.78 Hz granularity.  That timer can be adjusted
 * up to 1000 Hz using Windows multimedia library.
 *
 * @section su_timer_usage Using Timers
 *
 * A timer is created by calling su_timer_create():
 * @code
 *   timer = su_timer_create(su_root_task(root), 200);
 * @endcode
 * The default duration is given in milliseconds.
 *
 * Usually, timer wakeup function should be called at regular intervals. In
 * such case, the timer is activated using function su_timer_set_for_ever().
 * When the timer is activated it is given the wakeup function and pointer to
 * context data:
 * @code
 *   su_timer_set_for_ever(timer, timer_wakeup, args);
 * @endcode
 *
 * When the interval has passed, the root event loop calls the wakeup
 * function:
 * @code
 *   timer_wakeup(root, timer, args);
 * @endcode
 *
 * If the number of calls to callback function is important, use
 * su_timer_run() instead. The run timer tries to compensate for missed time
 * and invokes the callback function several times if needed. (Because the
 * real-time clock can be adjusted or the program suspended, e.g., while
 * debugged, the callback function can be called thousends of times in a
 * row.) Note that while the timer tries to compensate for delays occurred
 * before and during the callback, it cannot be used as an exact source of
 * timing information.
 *
 * Timer ceases running when su_timer_reset() is called.
 *
 * Alternatively, the timer can be @b set for one-time event invocation.
 * When the timer is set, it is given the wakeup function and pointer to
 * context data. The actual duration can also be specified using
 * su_timer_set_at(). @code su_timer_set(timer, timer_wakeup, args);
 * @endcode
 *
 * When the timer expires, the root event loop calls the wakeup function:
 * @code
 *   timer_wakeup(root, timer, args);
 * @endcode
 *
 * If the timed event is not needed anymore, the timer can be reset:
 * @code
 *   su_timer_reset(timer);
 * @endcode
 *
 * If the timer is expected to be called at regular intervals, it is
 * possible to set ro run continously with su_timer_run().  While such a
 * continously running timer is active it @b must @b not @b be @b set using
 * su_timer_set() or su_timer_set_at().
 *
 * When the timer is not needed anymore, the timer object itself should be
 * destroyed:
 * @code
 *   su_timer_destroy(timer);
 * @endcode
 */

struct su_timer_s {
  /** Pointers within red-black tree */
  su_timer_t     *sut_left, *sut_right, *sut_parent;
  su_task_r       sut_task;	/**< Task reference */
  su_time_t       sut_when;	/**< When timer should be waken up next time */
  su_duration_t   sut_duration;	/**< Timer duration */
  su_timer_f      sut_wakeup;	/**< Function to call when waken up */
  su_timer_arg_t *sut_arg;	/**< Pointer to argument data */
  su_time_t       sut_run;	/**< When this timer was last waken up */
  unsigned        sut_woken;	/**< Timer has waken up this many times */
  unsigned short  sut_running;	/**< Timer is running */

  unsigned char   sut_black;	/**< Black node */
  unsigned char   sut_set;	/**< Timer is set (inserted in tree) */
};

/** Timer running status */
enum sut_running {
  reset = 0,		/**< Timer is not running */
  run_at_intervals = 1, /**< Compensate missed wakeup calls */
  run_for_ever = 2	/**< Do not compensate  */
};

#define SU_TIMER_IS_SET(sut) ((sut)->sut_set)

/* Accessor macros for rbtree */
#define LEFT(sut) ((sut)->sut_left)
#define RIGHT(sut) ((sut)->sut_right)
#define PARENT(sut) ((sut)->sut_parent)
#define SET_RED(sut) ((sut)->sut_black = 0)
#define SET_BLACK(sut) ((sut)->sut_black = 1)
#define CMP(a, b) SU_TIME_CMP((a)->sut_when, (b)->sut_when)
#define IS_RED(sut) ((sut) && (sut)->sut_black == 0)
#define IS_BLACK(sut) (!(sut) || (sut)->sut_black == 1)
#define COPY_COLOR(dst, src) ((dst)->sut_black = (src)->sut_black)
#define INSERT(sut) ((sut)->sut_set = 1)
#define REMOVE(sut) ((sut)->sut_set = 0,				\
  (sut)->sut_left = (sut)->sut_right = (sut)->sut_parent = NULL)

RBTREE_PROTOS(su_inline, timers, su_timer_t);

su_inline int timers_append(su_timer_queue_t *, su_timer_t *);
su_inline void timers_remove(su_timer_queue_t *, su_timer_t *);
su_inline su_timer_t *timers_succ(su_timer_t const *);
su_inline su_timer_t *timers_prec(su_timer_t const *);
su_inline su_timer_t *timers_first(su_timer_t const *);
su_inline su_timer_t *timers_last(su_timer_t const *);

RBTREE_BODIES(su_inline, timers, su_timer_t,
	      LEFT, RIGHT, PARENT,
	      IS_RED, SET_RED, IS_BLACK, SET_BLACK, COPY_COLOR,
	      CMP, INSERT, REMOVE);

/**@internal Set the timer.
 *
 * @retval 0 when successful (always)
 */
su_inline int
su_timer_set0(su_timer_queue_t *timers,
	      su_timer_t *t,
	      su_timer_f wakeup,
	      su_wakeup_arg_t *arg,
	      su_time_t when,
	      su_duration_t offset)
{
  if (SU_TIMER_IS_SET(t))
    timers_remove(timers, t);

  t->sut_wakeup = wakeup;
  t->sut_arg = arg;
  t->sut_when = su_time_add(when, offset);

  return timers_append(timers, t);
}

/**@internal Reset the timer.
 *
 * @retval 0 when successful (always)
 */
su_inline int
su_timer_reset0(su_timer_queue_t *timers,
		su_timer_t *t)
{
  if (SU_TIMER_IS_SET(t))
    timers_remove(timers, t);

  t->sut_wakeup = NULL;
  t->sut_arg = NULL;
  t->sut_running = reset;

  memset(&t->sut_run, 0, sizeof(t->sut_run));

  return 0;
}

/**@internal Validate timer @a t and return pointer to per-port timer tree.
 *
 * @retval pointer to pointer to timer tree when successful
 * @retval NULL upon an error
 */
static
su_timer_queue_t *su_timer_tree(su_timer_t const *t,
				int use_sut_duration,
				char const *caller)
{
  su_timer_queue_t *timers;

  if (t == NULL) {
    SU_DEBUG_1(("%s(%p): %s\n", caller, (void *)t,
		"NULL argument"));
    return NULL;
  }

  timers = su_task_timers(t->sut_task);

  if (timers == NULL)
    SU_DEBUG_1(("%s(%p): %s\n", caller, (void *)t,
		"invalid timer"));

  if (use_sut_duration && t->sut_duration == 0) {
    assert(t->sut_duration > 0);
    SU_DEBUG_1(("%s(%p): %s\n", caller, (void *)t,
		"timer without default duration"));
    return NULL;
  }

  return timers;
}


/**Create a timer.
 *
 * Allocate and initialize an instance of su_timer_t.
 *
 * @param task a task for root object with which the timer will be associated
 * @param msec the default duration of the timer
 *
 * @return A pointer to allocated timer instance, NULL on error.
 */
su_timer_t *su_timer_create(su_task_r const task, su_duration_t msec)
{
  su_timer_t *retval;

  assert(msec >= 0);

  if (!su_task_cmp(task, su_task_null))
    return NULL;

  retval = su_zalloc(NULL, sizeof(*retval));
  if (retval) {
    su_task_copy(retval->sut_task, task);
    retval->sut_duration = msec;
  }

  return retval;
}

/** Destroy a timer.
 *
 * Deinitialize and free an instance of su_timer_t.
 *
 * @param t pointer to the timer object
 */
void su_timer_destroy(su_timer_t *t)
{
  if (t) {
    su_timer_queue_t *timers = su_task_timers(t->sut_task);
    if (timers)
      su_timer_reset0(timers, t);
    su_task_deinit(t->sut_task);
    su_free(NULL, t);
  }
}

/** Set the timer for the given @a interval.
 *
 *  Sets (starts) the given timer to expire after the specified duration.
 *
 * @param t       pointer to the timer object
 * @param wakeup  pointer to the wakeup function
 * @param arg     argument given to the wakeup function
 * @param interval duration in milliseconds before timer wakeup is called
 *
 * @return 0 if successful, -1 otherwise.
 */
int su_timer_set_interval(su_timer_t *t,
			  su_timer_f wakeup,
			  su_timer_arg_t *arg,
			  su_duration_t interval)
{
  su_timer_queue_t *timers = su_timer_tree(t, 0, "su_timer_set_interval");

  if (t == NULL)
    return -1;

  return su_timer_set0(timers, t, wakeup, arg, su_now(), interval);
}

/** Set the timer for the default interval.
 *
 *  Sets (starts) the given timer to expire after the default duration.
 *
 *  The timer must have an default duration.
 *
 * @param t       pointer to the timer object
 * @param wakeup  pointer to the wakeup function
 * @param arg     argument given to the wakeup function
 *
 * @return 0 if successful, -1 otherwise.
 */
int su_timer_set(su_timer_t *t,
		 su_timer_f wakeup,
		 su_timer_arg_t *arg)
{
  su_timer_queue_t *timers = su_timer_tree(t, 1, "su_timer_set");

  if (timers == NULL)
    return -1;

  return su_timer_set0(timers, t, wakeup, arg, su_now(), t->sut_duration);
}

/** Set timer at known time.
 *
 *  Sets the timer to expire at given time.
 *
 * @param t       pointer to the timer object
 * @param wakeup  pointer to the wakeup function
 * @param arg     argument given to the wakeup function
 * @param when    time structure defining the wakeup time
 *
 * @return 0 if successful, -1 otherwise.
 */
int su_timer_set_at(su_timer_t *t,
		    su_timer_f wakeup,
		    su_wakeup_arg_t *arg,
		    su_time_t when)
{
  su_timer_queue_t *timers = su_timer_tree(t, 0, "su_timer_set_at");

  if (timers == NULL)
    return -1;

  return su_timer_set0(timers, t, wakeup, arg, when, 0);
}

/** Set the timer for regular intervals.
 *
 * Run the given timer continuously, call wakeup function repeately in the
 * default interval. If a wakeup call is missed, try to make it up (in other
 * words, this kind of timer fails miserably if time is adjusted and it
 * should really use /proc/uptime instead of gettimeofday()).
 *
 * While a continously running timer is active it @b must @b not @b be @b
 * set using su_timer_set() or su_timer_set_at().
 *
 * The timer must have an non-zero default interval.
 *
 * @param t       pointer to the timer object
 * @param wakeup  pointer to the wakeup function
 * @param arg     argument given to the wakeup function
 *
 * @return 0 if successful, -1 otherwise.
 */
int su_timer_run(su_timer_t *t,
		 su_timer_f wakeup,
		 su_timer_arg_t *arg)
{
  su_timer_queue_t *timers = su_timer_tree(t, 1, "su_timer_run");
  su_time_t now;

  if (timers == NULL)
    return -1;

  t->sut_running = run_at_intervals;
  t->sut_run = now = su_now();
  t->sut_woken = 0;

  return su_timer_set0(timers, t, wakeup, arg, now, t->sut_duration);
}

/**Set the timer for regular intervals.
 *
 * Run the given timer continuously, call wakeup function repeately in the
 * default interval. While a continously running timer is active it @b must
 * @b not @b be @b set using su_timer_set() or su_timer_set_at(). Unlike
 * su_timer_run(), set for ever timer does not try to catchup missed
 * callbacks.
 *
 * The timer must have an non-zero default interval.
 *
 * @param t       pointer to the timer object
 * @param wakeup  pointer to the wakeup function
 * @param arg     argument given to the wakeup function
 *
 * @return 0 if successful, -1 otherwise.
 */
int su_timer_set_for_ever(su_timer_t *t,
			  su_timer_f wakeup,
			  su_timer_arg_t *arg)
{
  su_timer_queue_t *timers = su_timer_tree(t, 1, "su_timer_set_for_ever");
  su_time_t now;

  if (timers == NULL)
    return -1;

  t->sut_running = run_for_ever;
  t->sut_run = now = su_now();
  t->sut_woken = 0;

  return su_timer_set0(timers, t, wakeup, arg, now, t->sut_duration);
}

/**Reset the timer.
 *
 * Resets (stops) the given timer.
 *
 * @param t  pointer to the timer object
 *
 * @return 0 if successful, -1 otherwise.
 */
int su_timer_reset(su_timer_t *t)
{
  su_timer_queue_t *timers = su_timer_tree(t, 0, "su_timer_reset");

  if (timers == NULL)
    return -1;

  return su_timer_reset0(timers, t);
}

/** @internal Check for expired timers in queue.
 *
 * The function su_timer_expire() checks a timer queue and executes and
 * removes expired timers from the queue. It also calculates the time when
 * the next timer expires.
 *
 * @param timers   pointer to the timer queue
 * @param timeout  timeout in milliseconds [IN/OUT]
 * @param now      current timestamp
 *
 * @return
 * The number of expired timers.
 */
int su_timer_expire(su_timer_queue_t * const timers,
		    su_duration_t *timeout,
		    su_time_t now)
{
  su_timer_t *t;
  su_timer_f f;
  int n = 0;

  if (!*timers)
    return n;

  for (;;) {
    t = timers_first(*timers);

    if (t == NULL || SU_TIME_CMP(t->sut_when, now) > 0)
      break;

    timers_remove(timers, t);

    f = t->sut_wakeup; t->sut_wakeup = NULL;
    assert(f);

    if (t->sut_running == run_at_intervals) {
      while (t->sut_running == run_at_intervals &&
	     t->sut_duration > 0) {
	if (su_time_diff(t->sut_when, now) > 0) {
	  su_timer_set0(timers, t, f, t->sut_arg, t->sut_run, 0);
	  break;
	}
	t->sut_when = t->sut_run = su_time_add(t->sut_run, t->sut_duration);
	t->sut_woken++;
	f(su_root_magic(su_task_root(t->sut_task)), t, t->sut_arg), n++;
      }
    }
    else if (t->sut_running == run_for_ever) {
      t->sut_woken++;
      t->sut_when = now;
      f(su_root_magic(su_task_root(t->sut_task)), t, t->sut_arg), n++;
      if (t->sut_running == run_for_ever)
	su_timer_set0(timers, t, f, t->sut_arg, now, t->sut_duration);
    }
    else {
      t->sut_when = now;
      f(su_root_magic(su_task_root(t->sut_task)), t, t->sut_arg); n++;
    }
  }

  if (t) {
    su_duration_t at = su_duration(t->sut_when, now);

    if (at < *timeout)
      *timeout = at;
  }

  return n;
}


/** Calculate duration in milliseconds until next timer expires. */
su_duration_t su_timer_next_expires(su_timer_t const * t, su_time_t now)
{
  su_duration_t tout;

  t = timers_first(t);

  if (!t)
    return SU_DURATION_MAX;

  tout = su_duration(t->sut_when, now);

  return tout > 0 ? tout : 0 ;
}

/**
 * Resets and frees all timers belonging to a task.
 *
 * The function su_timer_destroy_all() resets and frees all timers belonging
 * to the specified task in the queue.
 *
 * @param timers   pointer to the timers
 * @param task     task owning the timers
 *
 * @return Number of timers reset.
 */
int su_timer_reset_all(su_timer_queue_t *timers, su_task_r task)
{
  su_timer_t *t, *t_next;
  int n = 0;

  if (!timers || !*timers)
    return 0;

  for (t = timers_first(*timers); t; t = t_next) {
    t_next = timers_succ(t);

    if (su_task_cmp(task, t->sut_task))
      continue;

    n++;
    timers_remove(timers, t);
    su_free(NULL, t);
  }

  return n;
}

/** Get the root object owning the timer.
 *
 * Return pointer to the root object owning the timer.
 *
 * @param t pointer to the timer
 *
 * @return Pointer to the root object.
 */
su_root_t *su_timer_root(su_timer_t const *t)
{
  return t ? su_task_root(t->sut_task) : NULL;
}
