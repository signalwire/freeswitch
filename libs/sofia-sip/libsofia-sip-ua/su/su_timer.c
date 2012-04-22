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

#include <sys/types.h>
#include "sofia-sip/heap.h"

typedef union {
  void *private;
  /* Use for debugging */
  struct timers_priv {
    size_t _size, _used;
    struct su_timer_s * _heap[2];
  } *actual;
} su_timer_heap_t;

#define SU_TIMER_QUEUE_T su_timer_heap_t

#include "sofia-sip/su.h"
#include "su_port.h"
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
  su_task_r       sut_task;	/**< Task reference */
  size_t          sut_set;	/**< Timer is set (inserted in heap) */
  su_time_t       sut_when;	/**< When timer should be waken up next time */
  su_duration_t   sut_duration;	/**< Timer duration */
  su_timer_f      sut_wakeup;	/**< Function to call when waken up */
  su_timer_arg_t *sut_arg;	/**< Pointer to argument data */
  unsigned        sut_woken;	/**< Timer has waken up this many times */

  unsigned        sut_running:2;/**< Timer is running */
  unsigned        sut_deferrable:1;/**< Timer can be deferrable */
};

/** Timer running status */
enum sut_running {
  reset = 0,		/**< Timer is not running */
  run_at_intervals = 1, /**< Compensate missed wakeup calls */
  run_for_ever = 2	/**< Do not compensate  */
};

#define SU_TIMER_IS_SET(sut) ((sut)->sut_set != 0)

HEAP_DECLARE(su_inline, su_timer_queue_t, timers_, su_timer_t *);

su_inline void timers_set(su_timer_t **array, size_t index, su_timer_t *t)
{
  array[t->sut_set = index] = t;
}

su_inline int timers_less(su_timer_t *a, su_timer_t *b)
{
  return
    a->sut_when.tv_sec < b->sut_when.tv_sec ||
    (a->sut_when.tv_sec == b->sut_when.tv_sec &&
     a->sut_when.tv_usec < b->sut_when.tv_usec);
}

su_inline void *timers_alloc(void *argument, void *memory, size_t size)
{
  (void)argument;

  if (size)
    return realloc(memory, size);
  else
    return free(memory), NULL;
}

HEAP_BODIES(su_inline, su_timer_queue_t, timers_, su_timer_t *,
	    timers_less, timers_set, timers_alloc, NULL);

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
  int retval;

  if (timers == NULL)
    return -1;

  if (SU_TIMER_IS_SET(t))
    timers_remove(timers[0], t->sut_set);

  t->sut_wakeup = wakeup;
  t->sut_arg = arg;
  t->sut_when = su_time_add(when, offset);

  if (timers_is_full(timers[0])) {
    timers_resize(NULL, timers, 0);
    assert(!timers_is_full(timers[0]));
    if (timers_is_full(timers[0]))
      return -1;
  }

  retval = timers_add(timers[0], t); assert(retval == 0);

  return retval;
}

/**@internal Validate timer @a t and return pointer to per-port timer tree.
 *
 * @retval pointer to pointer to timer tree when successful
 * @retval NULL upon an error
 */
static
su_timer_queue_t *su_timer_queue(su_timer_t const *t,
				 int use_sut_duration,
				 char const *caller)
{
  su_timer_queue_t *timers;

  if (t == NULL) {
    SU_DEBUG_1(("%s(%p): %s\n", caller, (void *)t,
		"NULL argument"));
    return NULL;
  }

  if (use_sut_duration && t->sut_duration == 0) {
    assert(t->sut_duration > 0);
    SU_DEBUG_1(("%s(%p): %s\n", caller, (void *)t,
		"timer without default duration"));
    return NULL;
  }

  if (t->sut_deferrable)
    timers = su_task_deferrable(t->sut_task);
  else
    timers = su_task_timers(t->sut_task);

  if (timers == NULL) {
    SU_DEBUG_1(("%s(%p): %s\n", caller, (void *)t, "invalid timer"));
    return NULL;
  }
  else if (timers_is_full(timers[0]) && timers_resize(NULL, timers, 0) == -1) {
    SU_DEBUG_1(("%s(%p): %s\n", caller, (void *)t, "timer queue failed"));
    return NULL;
  }

  return timers;
}


/**Create a timer.
 *
 * Allocate and initialize an instance of su_timer_t.
 *
 * @param task a task for root object with which the timer will be associated
 * @param msec the default duration of the timer in milliseconds
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
    su_timer_reset(t);
    su_task_deinit(t->sut_task);
    su_free(NULL, t);
  }
}

/** Check if the timer has been set.
 *
 * @param t pointer to a timer object
 *
 * @return Nonzero if set, zero if reset.
 *
 * @NEW_1_12_11
 */
int su_timer_is_set(su_timer_t const *t)
{
  return t && t->sut_set != 0;
}

/**Return when the timer has been last expired.
 *
 * @param t pointer to a timer object
 *
 * @return Timestamp (as returned by su_time()).
 *
 * @note If the timer is running (set with su_timer_run()), the returned
 * timestamp not the actual time but it is rather calculated from the
 * initial timestamp.
 *
 * @NEW_1_12_11
 */
su_time_t su_timer_latest(su_timer_t const *t)
{
  su_time_t tv = { 0, 0 };

  return t ? t->sut_when : tv;
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
  su_timer_queue_t *timers = su_timer_queue(t, 0, "su_timer_set_interval");

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
  su_timer_queue_t *timers = su_timer_queue(t, 1, "su_timer_set");

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
  su_timer_queue_t *timers = su_timer_queue(t, 0, "su_timer_set_at");

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
  su_timer_queue_t *timers = su_timer_queue(t, 1, "su_timer_run");

  if (timers == NULL)
    return -1;

  t->sut_running = run_at_intervals;
  t->sut_woken = 0;

  return su_timer_set0(timers, t, wakeup, arg, su_now(), t->sut_duration);
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
  su_timer_queue_t *timers = su_timer_queue(t, 1, "su_timer_set_for_ever");

  if (timers == NULL)
    return -1;

  t->sut_running = run_for_ever;
  t->sut_woken = 0;

  return su_timer_set0(timers, t, wakeup, arg, su_now(), t->sut_duration);
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
  su_timer_queue_t *timers = su_timer_queue(t, 0, "su_timer_reset");

  if (timers == NULL)
    return -1;

  if (SU_TIMER_IS_SET(t))
    timers_remove(timers[0], t->sut_set);

  t->sut_wakeup = NULL;
  t->sut_arg = NULL;
  t->sut_running = reset;

  return 0;
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

  if (timers_used(timers[0]) == 0)
    return 0;

  while ((t = timers_get(timers[0], 1))) {
    if (SU_TIME_CMP(t->sut_when, now) > 0) {
      su_duration_t at = su_duration(t->sut_when, now);

      if (at < *timeout || *timeout < 0)
	*timeout = at;

      break;
    }

    timers_remove(timers[0], 1);

    f = t->sut_wakeup; t->sut_wakeup = NULL;
    assert(f);

    if (t->sut_running == run_at_intervals) {
      while (t->sut_running == run_at_intervals &&
	     t->sut_set == 0 &&
	     t->sut_duration > 0) {
	if (su_time_diff(t->sut_when, now) > 0) {
	  su_timer_set0(timers, t, f, t->sut_arg, t->sut_when, 0);
	  break;
	}
	t->sut_when = su_time_add(t->sut_when, t->sut_duration);
	t->sut_woken++;
	f(su_root_magic(su_task_root(t->sut_task)), t, t->sut_arg), n++;
      }
    }
    else if (t->sut_running == run_for_ever) {
      t->sut_woken++;
      t->sut_when = now;
      f(su_root_magic(su_task_root(t->sut_task)), t, t->sut_arg), n++;
      if (t->sut_running == run_for_ever && t->sut_set == 0)
	su_timer_set0(timers, t, f, t->sut_arg, now, t->sut_duration);
    }
    else {
      t->sut_when = now;
      f(su_root_magic(su_task_root(t->sut_task)), t, t->sut_arg); n++;
    }
  }

  return n;
}


/** Calculate duration in milliseconds until next timer expires. */
su_duration_t su_timer_next_expires(su_timer_queue_t const *timers,
				    su_time_t now)
{
  su_duration_t next = SU_DURATION_MAX;

  su_timer_t const *t;

  t = timers ? timers_get(timers[0], 1) : NULL;

  if (t) {
    next = su_duration(t->sut_when, now);
    if (next < 0)
      next = 0;
  }

  return next;
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
  size_t i;
  int n = 0;

  if (!timers)
    return 0;

  timers_sort(timers[0]);

  for (i = timers_used(timers[0]); i > 0; i--) {
    su_timer_t *t = timers_get(timers[0], i);

    if (su_task_cmp(task, t->sut_task))
      continue;

    timers_remove(timers[0], i);

    su_free(NULL, t);
    n++;
  }

  if (!timers_used(timers[0]))
    free(timers->private), timers->private = NULL;

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


/** Change timer as deferrable (or as undeferrable).
 *
 * A deferrable timer is executed after the given timeout, however, the task
 * tries to avoid being woken up only because the timeout. Deferable timers
 * have their own queue and timers there are ignored when calculating the
 * timeout for epoll()/select()/whatever unless the timeout would exceed the
 * maximum defer time. The maximum defer time is 15 seconds by default, but
 * it can be modified by su_root_set_max_defer().
 *
 * @param t pointer to the timer
 * @param value make timer deferrable if true, undeferrable if false
 *
 * @return 0 if succesful, -1 upon an error
 *
 * @sa su_root_set_max_defer()
 *
 * @NEW_1_12_7
 */
int su_timer_deferrable(su_timer_t *t, int value)
{
  if (t == NULL || su_task_deferrable(t->sut_task) == NULL)
    return errno = EINVAL, -1;

  t->sut_deferrable = value != 0;
 
  return 0;
}
