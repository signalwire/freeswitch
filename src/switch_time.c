/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
 *
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthm@freeswitch.org>
 * Massimo Cetra <devel@navynet.it> - Timezone functionality
 *
 *
 * softtimer.c -- Software Timer Module
 *
 */

#include <switch.h>
#include <stdio.h>
#include "private/switch_core_pvt.h"

#ifdef TIMERFD_WRAP
#include <timerfd_wrap.h>
#ifndef HAVE_TIMERFD_CREATE
#define HAVE_TIMERFD_CREATE
#endif
#else
#ifdef HAVE_TIMERFD_CREATE
#include <sys/timerfd.h>
#endif
#endif

//#if defined(DARWIN)
#define DISABLE_1MS_COND
//#endif

#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffff
#endif

#define MAX_TICK UINT32_MAX - 1024

#define MAX_ELEMENTS 3600
#define IDLE_SPEED 100

/* In Windows, enable the montonic timer for better timer accuracy,
 * GetSystemTimeAsFileTime does not update on timeBeginPeriod on these OS.
 * Flag SCF_USE_WIN32_MONOTONIC must be enabled to activate it (start parameter -monotonic-clock).
 */

#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
static int MONO = 1;
#else
static int MONO = 0;
#endif


static int SYSTEM_TIME = 0;

/* clock_nanosleep works badly on some kernels but really well on others.
   timerfd seems to work well as long as it exists so if you have timerfd we'll also enable clock_nanosleep by default.
*/
#if defined(HAVE_TIMERFD_CREATE)
static int TFD = 1;
#if defined(HAVE_CLOCK_NANOSLEEP)
static int NANO = 1;
#else
static int NANO = 0;
#endif
#else
static int TFD = 0;
static int NANO = 0;
#endif

static int OFFSET = 0;

static int COND = 1;

static int MATRIX = 1;

#ifdef WIN32
static CRITICAL_SECTION timer_section;
static switch_time_t win32_tick_time_since_start = -1;
static DWORD win32_last_get_time_tick = 0;

static uint8_t win32_use_qpc = 0;
static uint64_t win32_qpc_freq = 0;
#endif

static switch_memory_pool_t *module_pool = NULL;

static struct {
	int32_t RUNNING;
	int32_t STARTED;
	int32_t use_cond_yield;
	switch_mutex_t *mutex;
	uint32_t timer_count;
} globals;

#ifdef WIN32
#undef SWITCH_MOD_DECLARE_DATA
#define SWITCH_MOD_DECLARE_DATA __declspec(dllexport)
#endif

SWITCH_MODULE_LOAD_FUNCTION(softtimer_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(softtimer_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(softtimer_runtime);
SWITCH_MODULE_DEFINITION(CORE_SOFTTIMER_MODULE, softtimer_load, softtimer_shutdown, softtimer_runtime);

struct timer_private {
	switch_size_t reference;
	switch_size_t start;
	uint32_t roll;
	uint32_t ready;
};
typedef struct timer_private timer_private_t;

struct timer_matrix {
	switch_size_t tick;
	uint32_t count;
	uint32_t roll;
	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;
	switch_thread_rwlock_t *rwlock;
};
typedef struct timer_matrix timer_matrix_t;

static timer_matrix_t TIMER_MATRIX[MAX_ELEMENTS + 1];

static switch_time_t time_now(int64_t offset);

SWITCH_DECLARE(void) switch_os_yield(void)
{
#if defined(WIN32)
	SwitchToThread();
#else
	sched_yield();
#endif
}

static void do_sleep(switch_interval_time_t t)
{
#if defined(HAVE_CLOCK_NANOSLEEP) || defined(DARWIN)
	struct timespec ts;
#endif

#if defined(WIN32)
	if (t < 1000) {
		t = 1000;
	}
#endif

#if !defined(DARWIN)
	if (t > 100000 || !NANO) {
		apr_sleep(t);
		return;
	}
#endif

#if defined(HAVE_CLOCK_NANOSLEEP)
	t -= OFFSET;
	ts.tv_sec = t / 1000000;
	ts.tv_nsec = ((t % 1000000) * 1000);
	clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, NULL);

#elif defined(DARWIN)
	ts.tv_sec = t / APR_USEC_PER_SEC;
	ts.tv_nsec = (t % APR_USEC_PER_SEC) * 1000;
	nanosleep(&ts, NULL);
#else
	apr_sleep(t);
#endif

#if defined(DARWIN)
	sched_yield();
#endif

}

static switch_interval_time_t average_time(switch_interval_time_t t, int reps)
{
	int x = 0;
	switch_time_t start, stop, sum = 0;

	for (x = 0; x < reps; x++) {
		start = switch_time_ref();
		do_sleep(t);
		stop = switch_time_ref();
		sum += (stop - start);
	}

	return sum / reps;

}

#define calc_step() if (step > 11) step -= 10; else if (step > 1) step--
SWITCH_DECLARE(void) switch_time_calibrate_clock(void)
{
	int x;
	switch_interval_time_t avg, val = 1000, want = 1000;
	int over = 0, under = 0, good = 0, step = 50, diff = 0, retry = 0, lastgood = 0, one_k = 0;

#ifdef HAVE_CLOCK_GETRES
	struct timespec ts;
	long res = 0;
	clock_getres(CLOCK_MONOTONIC, &ts);
	res = ts.tv_nsec / 1000;


	if (res > 900 && res < 1100) {
		one_k = 1;
	}
	
	if (res > 1500) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
						  "Timer resolution of %ld microseconds detected!\n"
						  "Do you have your kernel timer frequency set to lower than 1,000Hz? "
						  "You may experience audio problems. Step MS %d\n", ts.tv_nsec / 1000, runtime.microseconds_per_tick / 1000);
		do_sleep(5000000);
		switch_time_set_cond_yield(SWITCH_TRUE);
		return;
	}
#endif

  top:
	val = 1000;
	step = 50;
	over = under = good = 0;
	OFFSET = 0;

	for (x = 0; x < 100; x++) {
		avg = average_time(val, 50);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Test: %ld Average: %ld Step: %d\n", (long) val, (long) avg, step);

		diff = abs((int) (want - avg));
		if (diff > 1500) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "Abnormally large timer gap %d detected!\n"
							  "Do you have your kernel timer frequency set to lower than 1,000Hz? You may experience audio problems.\n", diff);
			do_sleep(5000000);
			switch_time_set_cond_yield(SWITCH_TRUE);
			return;
		}

		if (diff <= 100) {
			lastgood = (int) val;
		}

		if (diff <= 2) {
			under = over = 0;
			lastgood = (int) val;
			if (++good > 10) {
				break;
			}
		} else if (avg > want) {
			if (under) {
				calc_step();
			}
			under = good = 0;
			if ((val - step) < 0) {
				if (++retry > 2)
					break;
				goto top;
			}
			val -= step;
			over++;
		} else if (avg < want) {
			if (over) {
				calc_step();
			}
			over = good = 0;
			if ((val - step) < 0) {
				if (++retry > 2)
					break;
				goto top;
			}
			val += step;
			under++;
		}
	}

	if (good >= 10) {
		OFFSET = (int) (want - val);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Timer offset of %d calculated\n", OFFSET);
	} else if (lastgood) {
		OFFSET = (int) (want - lastgood);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Timer offset of %d calculated (fallback)\n", OFFSET);
		switch_time_set_cond_yield(SWITCH_TRUE);
	} else if (one_k) {
		OFFSET = 900;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Timer offset CANNOT BE DETECTED, forcing OFFSET to 900\n");
		switch_time_set_cond_yield(SWITCH_TRUE);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Timer offset NOT calculated\n");
		switch_time_set_cond_yield(SWITCH_TRUE);
	}
}


SWITCH_DECLARE(switch_time_t) switch_micro_time_now(void)
{
	return (globals.RUNNING == 1 && runtime.timestamp) ? runtime.timestamp : switch_time_now();
}

SWITCH_DECLARE(switch_time_t) switch_mono_micro_time_now(void)
{
	return time_now(-1);
}


SWITCH_DECLARE(time_t) switch_epoch_time_now(time_t *t)
{
	time_t now = switch_micro_time_now() / APR_USEC_PER_SEC;
	if (t) {
		*t = now;
	}
	return now;
}

SWITCH_DECLARE(void) switch_time_set_monotonic(switch_bool_t enable)
{
#if (defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)) || defined(WIN32)
	MONO = enable ? 1 : 0;
	switch_time_sync();
#else
	MONO = 0;
#endif
}


SWITCH_DECLARE(void) switch_time_set_use_system_time(switch_bool_t enable)
{
	SYSTEM_TIME = enable;
}


SWITCH_DECLARE(void) switch_time_set_timerfd(switch_bool_t enable)
{
#if defined(HAVE_TIMERFD_CREATE)
	TFD = enable ? 1 : 0;
	switch_time_sync();

#else
	TFD = 0;
#endif
}


SWITCH_DECLARE(void) switch_time_set_matrix(switch_bool_t enable)
{
	MATRIX = enable ? 1 : 0;
	switch_time_sync();
}

SWITCH_DECLARE(void) switch_time_set_nanosleep(switch_bool_t enable)
{
#if defined(HAVE_CLOCK_NANOSLEEP)
	NANO = enable ? 1 : 0;
#endif
}

SWITCH_DECLARE(void) switch_time_set_cond_yield(switch_bool_t enable)
{
	COND = enable ? 1 : 0;
	if (COND) {
		MATRIX = 1;
	}
	switch_time_sync();
}

static switch_time_t time_now(int64_t offset)
{
	switch_time_t now;

#if (defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)) || defined(WIN32)
	if (MONO) {
#ifndef WIN32
		struct timespec ts;
		clock_gettime(offset ? CLOCK_MONOTONIC : CLOCK_REALTIME, &ts);
		if (offset < 0) offset = 0;
		now = ts.tv_sec * APR_USEC_PER_SEC + (ts.tv_nsec / 1000) + offset;
#else
		if (offset == 0) {
			return switch_time_now();
		} else if (offset < 0) offset = 0;
		

		if (win32_use_qpc) {
			/* Use QueryPerformanceCounter */
			uint64_t count = 0;
			QueryPerformanceCounter((LARGE_INTEGER*)&count);
			now = ((count * 1000000) / win32_qpc_freq) + offset;
		} else {
			/* Use good old timeGetTime() */
			DWORD tick_now;
			DWORD tick_diff;

			tick_now = timeGetTime();
			if (win32_tick_time_since_start != -1) {
				EnterCriticalSection(&timer_section);
				/* just add diff (to make it work more than 50 days). */
				tick_diff = tick_now - win32_last_get_time_tick;
				win32_tick_time_since_start += tick_diff;

				win32_last_get_time_tick = tick_now;
				now = (win32_tick_time_since_start * 1000) + offset;
				LeaveCriticalSection(&timer_section);
			} else {
				/* If someone is calling us before timer is initialized,
				 * return the current tick + offset
				 */
				now = (tick_now * 1000) + offset;
			}
		}
#endif
	} else {
#endif
		now = switch_time_now();

#if (defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)) || defined(WIN32)
	}
#endif

	return now;
}

SWITCH_DECLARE(switch_time_t) switch_time_ref(void)
{
	if (SYSTEM_TIME) {
		/* Return system time reference */
		return time_now(0);
	} else {
		/* Return monotonic time reference (when available) */
		return time_now(-1);
	}
}

static switch_time_t last_time = 0;

SWITCH_DECLARE(void) switch_time_sync(void)
{
	runtime.time_sync++; /* Indicate that we are syncing time right now */

	runtime.reference = switch_time_now();

	if (SYSTEM_TIME) {
		runtime.reference = time_now(0);
		runtime.mono_reference = time_now(-1);
		runtime.offset = 0;
	} else {
		runtime.offset = runtime.reference - time_now(-1); /* Get the offset between system time and the monotonic clock (when available) */
		runtime.reference = time_now(runtime.offset);
	}


	if (runtime.reference - last_time > 1000000 || last_time == 0) {
		if (SYSTEM_TIME) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Clock is already configured to always report system time.\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Clock synchronized to system time.\n");
		}
	}
	last_time = runtime.reference;

	runtime.time_sync++; /* Indicate that we are finished syncing time */
}

SWITCH_DECLARE(void) switch_micro_sleep(switch_interval_time_t t)
{
	do_sleep(t);
}

SWITCH_DECLARE(void) switch_sleep(switch_interval_time_t t)
{

	if (globals.RUNNING != 1 || t < 1000 || t >= 10000) {
		do_sleep(t);
		return;
	}
#ifndef DISABLE_1MS_COND
	if (globals.use_cond_yield == 1) {
		switch_cond_yield(t);
		return;
	}
#endif

	do_sleep(t);
}


SWITCH_DECLARE(void) switch_cond_next(void)
{
	if (runtime.tipping_point && globals.timer_count >= runtime.tipping_point) {
		switch_os_yield();
		return;
	}
#ifdef DISABLE_1MS_COND
	do_sleep(1000);
#else
	if (globals.RUNNING != 1 || !runtime.timestamp || globals.use_cond_yield != 1) {
		do_sleep(1000);
		return;
	}
	switch_mutex_lock(TIMER_MATRIX[1].mutex);
	switch_thread_cond_wait(TIMER_MATRIX[1].cond, TIMER_MATRIX[1].mutex);
	switch_mutex_unlock(TIMER_MATRIX[1].mutex);
#endif
}

SWITCH_DECLARE(void) switch_cond_yield(switch_interval_time_t t)
{
	switch_time_t want;
	if (!t)
		return;

	if (globals.RUNNING != 1 || !runtime.timestamp || globals.use_cond_yield != 1) {
		do_sleep(t);
		return;
	}
	want = runtime.timestamp + t;
	while (globals.RUNNING == 1 && globals.use_cond_yield == 1 && runtime.timestamp < want) {
		switch_mutex_lock(TIMER_MATRIX[1].mutex);
		if (runtime.timestamp < want) {
			switch_thread_cond_wait(TIMER_MATRIX[1].cond, TIMER_MATRIX[1].mutex);
		}
		switch_mutex_unlock(TIMER_MATRIX[1].mutex);
	}


}

static switch_status_t timer_init(switch_timer_t *timer)
{
	timer_private_t *private_info;
	int sanity = 0;

	while (globals.STARTED == 0) {
		do_sleep(100000);
		if (++sanity == 300) {
			abort();
		}
	}

	if (globals.RUNNING != 1 || !globals.mutex || timer->interval < 1) {
		return SWITCH_STATUS_FALSE;
	}

	if ((private_info = switch_core_alloc(timer->memory_pool, sizeof(*private_info)))) {
		switch_mutex_lock(globals.mutex);
		if (!TIMER_MATRIX[timer->interval].mutex) {
			switch_mutex_init(&TIMER_MATRIX[timer->interval].mutex, SWITCH_MUTEX_NESTED, module_pool);
			switch_thread_cond_create(&TIMER_MATRIX[timer->interval].cond, module_pool);
		}
		TIMER_MATRIX[timer->interval].count++;
		switch_mutex_unlock(globals.mutex);
		timer->private_info = private_info;
		private_info->start = private_info->reference = TIMER_MATRIX[timer->interval].tick;
		private_info->start -= 2; /* switch_core_timer_init sets samplecount to samples, this makes first next() step once */
		private_info->roll = TIMER_MATRIX[timer->interval].roll;
		private_info->ready = 1;

		if ((timer->interval == 10 || timer->interval == 30) && runtime.microseconds_per_tick > 10000) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Increasing global timer resolution to 10ms to handle interval %d\n", timer->interval);
			runtime.microseconds_per_tick = 10000;
		}

		if (timer->interval > 0 && (timer->interval < (int)(runtime.microseconds_per_tick / 1000) || (timer->interval % 10) != 0)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Increasing global timer resolution to 1ms to handle interval %d\n", timer->interval);
			runtime.microseconds_per_tick = 1000;
			switch_time_sync();
		}

		switch_mutex_lock(globals.mutex);
		globals.timer_count++;
		if (runtime.tipping_point && globals.timer_count == (runtime.tipping_point + 1)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Crossed tipping point of %u, shifting into high-gear.\n", runtime.tipping_point);
		}
		switch_mutex_unlock(globals.mutex);

		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

#define check_roll() if (private_info->roll < TIMER_MATRIX[timer->interval].roll) {	\
		private_info->roll++;											\
		private_info->reference = private_info->start = TIMER_MATRIX[timer->interval].tick;	\
		private_info->start--; /* Must have a diff */					\
	}																	\


static switch_status_t timer_step(switch_timer_t *timer)
{
	timer_private_t *private_info = timer->private_info;
	uint64_t samples;

	if (globals.RUNNING != 1 || private_info->ready == 0) {
		return SWITCH_STATUS_FALSE;
	}

	check_roll();
	samples = timer->samples * (private_info->reference - private_info->start);

	if (samples > UINT32_MAX) {
		private_info->start = private_info->reference - 1; /* Must have a diff */
		samples = timer->samples;
	}

	timer->samplecount = (uint32_t) samples;
	private_info->reference++;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t timer_sync(switch_timer_t *timer)
{
	timer_private_t *private_info = timer->private_info;

	if (globals.RUNNING != 1 || private_info->ready == 0) {
		return SWITCH_STATUS_FALSE;
	}

	/* sync the clock */
	private_info->reference = timer->tick = TIMER_MATRIX[timer->interval].tick;

	/* apply timestamp */
	timer_step(timer);

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t timer_next(switch_timer_t *timer)
{
	timer_private_t *private_info = timer->private_info;

#ifdef DISABLE_1MS_COND
	int cond_index = timer->interval;
#else
	int cond_index = 1;
#endif
	int delta = (int) (private_info->reference - TIMER_MATRIX[timer->interval].tick);

	/* sync up timer if it's not been called for a while otherwise it will return instantly several times until it catches up */
	if (delta < -1) {
		private_info->reference = timer->tick = TIMER_MATRIX[timer->interval].tick;
	}
	timer_step(timer);

	if (!MATRIX) {
		do_sleep(1000 * timer->interval);
		goto end;
	}

	while (globals.RUNNING == 1 && private_info->ready && TIMER_MATRIX[timer->interval].tick < private_info->reference) {
		check_roll();

		if (runtime.tipping_point && globals.timer_count >= runtime.tipping_point) {
			switch_os_yield();
			globals.use_cond_yield = 0;
		} else {
			if (globals.use_cond_yield == 1) {
				switch_mutex_lock(TIMER_MATRIX[cond_index].mutex);
				if (TIMER_MATRIX[timer->interval].tick < private_info->reference) {
					switch_thread_cond_wait(TIMER_MATRIX[cond_index].cond, TIMER_MATRIX[cond_index].mutex);
				}
				switch_mutex_unlock(TIMER_MATRIX[cond_index].mutex);
			} else {
				do_sleep(1000);
			}
		}
	}

  end:
	return globals.RUNNING == 1 ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

static switch_status_t timer_check(switch_timer_t *timer, switch_bool_t step)
{
	timer_private_t *private_info = timer->private_info;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if (globals.RUNNING != 1 || !private_info->ready) {
		return SWITCH_STATUS_SUCCESS;
	}

	check_roll();

	timer->tick = TIMER_MATRIX[timer->interval].tick;

	if (timer->tick < private_info->reference) {
		timer->diff = private_info->reference - timer->tick;
	} else {
		timer->diff = 0;
	}

	if (timer->diff) {
		status = SWITCH_STATUS_FALSE;
	} else if (step) {
		timer_step(timer);
	}


	return status;
}

static switch_status_t timer_destroy(switch_timer_t *timer)
{
	timer_private_t *private_info = timer->private_info;
	if (timer->interval < MAX_ELEMENTS) {
		switch_mutex_lock(globals.mutex);
		TIMER_MATRIX[timer->interval].count--;
		if (TIMER_MATRIX[timer->interval].count == 0) {
			TIMER_MATRIX[timer->interval].tick = 0;
		}
		switch_mutex_unlock(globals.mutex);
	}
	if (private_info) {
		private_info->ready = 0;
	}

	switch_mutex_lock(globals.mutex);
	if (globals.timer_count) {
		globals.timer_count--;
		if (runtime.tipping_point && globals.timer_count == (runtime.tipping_point - 1)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Fell Below tipping point of %u, shifting into low-gear.\n", runtime.tipping_point);
		}
	}
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_SUCCESS;
}

static void win32_init_timers(void)
{
#ifdef WIN32
	OSVERSIONINFOEX version_info; /* Used to fetch current OS version from Windows */

	EnterCriticalSection(&timer_section);

	ZeroMemory(&version_info, sizeof(OSVERSIONINFOEX));
	version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

	/* Check if we should use timeGetTime() (pre-Vista) or QueryPerformanceCounter() (Vista and later) */

	if (GetVersionEx((OSVERSIONINFO*) &version_info)) {
		if (version_info.dwPlatformId == VER_PLATFORM_WIN32_NT && version_info.dwMajorVersion >= 6) {
			if (QueryPerformanceFrequency((LARGE_INTEGER*)&win32_qpc_freq) && win32_qpc_freq > 0) {
				/* At least Vista, and QueryPerformanceFrequency() suceeded, enable qpc */
				win32_use_qpc = 1;
			} else {
				/* At least Vista, but QueryPerformanceFrequency() failed, disable qpc */
				win32_use_qpc = 0;
			}
		} else {
			/* Older then Vista, disable qpc */
			win32_use_qpc = 0;
		}
	} else {
		/* Unknown version - we want at least Vista, disable qpc */
		win32_use_qpc = 0;
	}

	if (win32_use_qpc) {
		uint64_t count = 0;

		if (!QueryPerformanceCounter((LARGE_INTEGER*)&count) || count == 0) {
			/* Call to QueryPerformanceCounter() failed, disable qpc again */
			win32_use_qpc = 0;
		}
	}

	if (!win32_use_qpc) {
		/* This will enable timeGetTime() instead, qpc init failed */
		win32_last_get_time_tick = timeGetTime();
		win32_tick_time_since_start = win32_last_get_time_tick;
	}

	LeaveCriticalSection(&timer_section);
#endif
}

SWITCH_MODULE_RUNTIME_FUNCTION(softtimer_runtime)
{
	switch_time_t too_late = runtime.microseconds_per_tick * 1000;
	uint32_t current_ms = 0;
	uint32_t x, tick = 0;
	switch_time_t ts = 0, last = 0;
	int fwd_errs = 0, rev_errs = 0;
	int profile_tick = 0;
	int tfd = -1;
	uint32_t time_sync = runtime.time_sync;

#ifdef HAVE_TIMERFD_CREATE
	int last_MICROSECONDS_PER_TICK = runtime.microseconds_per_tick;

	struct itimerspec spec = { { 0 } };

	if (MONO && TFD) {
		tfd = timerfd_create(CLOCK_MONOTONIC, 0);

		if (tfd > -1) {
			spec.it_interval.tv_sec = 0;
			spec.it_interval.tv_nsec = runtime.microseconds_per_tick * 1000;
			spec.it_value.tv_sec = spec.it_interval.tv_sec;
			spec.it_value.tv_nsec = spec.it_interval.tv_nsec;
		
			if (timerfd_settime(tfd, TFD_TIMER_ABSTIME, &spec, NULL)) {
				close(tfd);
				tfd = -1;
			}
		}
	}
#else
	tfd = -1;
#endif

	runtime.profile_timer = switch_new_profile_timer();
	switch_get_system_idle_time(runtime.profile_timer, &runtime.profile_time);

	if (runtime.timer_affinity > -1) { 
		switch_core_thread_set_cpu_affinity(runtime.timer_affinity);
	}

	switch_time_sync();
	time_sync = runtime.time_sync;

	globals.STARTED = globals.RUNNING = 1;
	switch_mutex_lock(runtime.throttle_mutex);
	runtime.sps = runtime.sps_total;
	switch_mutex_unlock(runtime.throttle_mutex);

	if (MONO) {
		int loops;
		for (loops = 0; loops < 3; loops++) {
			ts = switch_time_ref();
			/* if it returns the same value every time it won't be of much use. */
			if (ts == last) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Broken MONOTONIC Clock Detected!, Support Disabled.\n");
				MONO = 0;
				NANO = 0;
				runtime.reference = switch_time_now();
				runtime.initiated = runtime.reference;
				break;
			}
			do_sleep(runtime.microseconds_per_tick);
			last = ts;
		}
	}

	ts = 0;
	last = 0;
	fwd_errs = rev_errs = 0;

#ifndef DISABLE_1MS_COND
	if (!NANO) {
		switch_mutex_init(&TIMER_MATRIX[1].mutex, SWITCH_MUTEX_NESTED, module_pool);
		switch_thread_cond_create(&TIMER_MATRIX[1].cond, module_pool);
	}
#endif


	switch_time_sync();
	time_sync = runtime.time_sync;

	globals.use_cond_yield = COND;
	globals.RUNNING = 1;

	while (globals.RUNNING == 1) {

#ifdef HAVE_TIMERFD_CREATE
		if (last_MICROSECONDS_PER_TICK != runtime.microseconds_per_tick) {
			spec.it_interval.tv_nsec = runtime.microseconds_per_tick * 1000;
			timerfd_settime(tfd, TFD_TIMER_ABSTIME, &spec, NULL);
		}
		
		last_MICROSECONDS_PER_TICK = runtime.microseconds_per_tick;
#endif

		runtime.reference += runtime.microseconds_per_tick;

		while (((ts = time_now(runtime.offset)) + 100) < runtime.reference) {
			if (ts < last) {
				if (MONO) {
					if (time_sync == runtime.time_sync) { /* Only resync if not in the middle of switch_time_sync() already */
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Virtual Migration Detected! Syncing Clock\n");
						win32_init_timers(); /* Make sure to reinit timers on WIN32 */
						switch_time_sync();
						time_sync = runtime.time_sync;
					}
				} else {
					int64_t diff = (int64_t) (ts - last);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Reverse Clock Skew Detected!\n");
					runtime.reference = switch_time_now();
					current_ms = 0;
					tick = 0;
					runtime.initiated += diff;
					rev_errs++;
				}

				if (!MONO || time_sync == runtime.time_sync) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT,
									  "If you see this message many times try setting the param enable-clock-nanosleep to true in switch.conf.xml or consider a nicer machine to run me on. I AM *FREE* afterall.\n");
				}
			} else {
				rev_errs = 0;
			}

			if (runtime.tipping_point && globals.timer_count >= runtime.tipping_point) {
				switch_os_yield();
			} else {
				if (tfd > -1 && globals.RUNNING == 1) {
					uint64_t exp;
					int r;
					r = read(tfd, &exp, sizeof(exp));
					r++;
				} else {
					switch_time_t timediff = runtime.reference - ts;

					if (runtime.microseconds_per_tick < timediff) {
						/* Only sleep for runtime.microseconds_per_tick if this value is lower then the actual time diff we need to sleep */
						do_sleep(runtime.microseconds_per_tick);
					} else {
#ifdef WIN32
						/* Windows only sleeps in ms precision, try to round the usec value as good as possible */
						do_sleep((switch_interval_time_t)floor((timediff / 1000.0) + 0.5) * 1000);
#else
						do_sleep(timediff);
#endif
					}
				}
			}

			last = ts;
		}

		if (ts > (runtime.reference + too_late)) {
			if (MONO) {
				if (time_sync == runtime.time_sync) { /* Only resync if not in the middle of switch_time_sync() already */
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Virtual Migration Detected! Syncing Clock\n");
					win32_init_timers(); /* Make sure to reinit timers on WIN32 */
					switch_time_sync();
					time_sync = runtime.time_sync;
				}
			} else {
				switch_time_t diff = ts - runtime.reference - runtime.microseconds_per_tick;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Forward Clock Skew Detected!\n");
				fwd_errs++;
				runtime.reference = switch_time_now();
				current_ms = 0;
				tick = 0;
				runtime.initiated += diff;
			}
		} else {
			fwd_errs = 0;
		}

		if (fwd_errs > 9 || rev_errs > 9) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Auto Re-Syncing clock.\n");
			switch_time_sync();
			time_sync = runtime.time_sync;
			fwd_errs = rev_errs = 0;
		}

		runtime.timestamp = ts;
		current_ms += (runtime.microseconds_per_tick / 1000);
		tick++;

		if (time_sync < runtime.time_sync) {
			time_sync++; /* Only step once for each loop, we want to make sure to keep this thread safe */
		}

		if (tick >= (1000000 / runtime.microseconds_per_tick)) {
			if (++profile_tick == 1) {
				switch_get_system_idle_time(runtime.profile_timer, &runtime.profile_time);
				profile_tick = 0;
			}
			
			if (runtime.sps <= 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Over Session Rate of %d!\n", runtime.sps_total);
			}
			switch_mutex_lock(runtime.throttle_mutex);
			runtime.sps_last = runtime.sps_total - runtime.sps;
			runtime.sps = runtime.sps_total;
			switch_mutex_unlock(runtime.throttle_mutex);
			tick = 0;
		}
#ifndef DISABLE_1MS_COND
		TIMER_MATRIX[1].tick++;
		if (switch_mutex_trylock(TIMER_MATRIX[1].mutex) == SWITCH_STATUS_SUCCESS) {
			switch_thread_cond_broadcast(TIMER_MATRIX[1].cond);
			switch_mutex_unlock(TIMER_MATRIX[1].mutex);
		}
		if (TIMER_MATRIX[1].tick == MAX_TICK) {
			TIMER_MATRIX[1].tick = 0;
			TIMER_MATRIX[1].roll++;
		}
#endif


		if (MATRIX && (current_ms % (runtime.microseconds_per_tick / 1000)) == 0) {
			for (x = (runtime.microseconds_per_tick / 1000); x <= MAX_ELEMENTS; x += (runtime.microseconds_per_tick / 1000)) {
				if ((current_ms % x) == 0) {
					if (TIMER_MATRIX[x].count) {
						TIMER_MATRIX[x].tick++;
#ifdef DISABLE_1MS_COND

						if (TIMER_MATRIX[x].mutex && switch_mutex_trylock(TIMER_MATRIX[x].mutex) == SWITCH_STATUS_SUCCESS) {
							switch_thread_cond_broadcast(TIMER_MATRIX[x].cond);
							switch_mutex_unlock(TIMER_MATRIX[x].mutex);
						}
#endif
						if (TIMER_MATRIX[x].tick == MAX_TICK) {
							TIMER_MATRIX[x].tick = 0;
							TIMER_MATRIX[x].roll++;
						}
					}
				}
			}
		}

		if (current_ms == MAX_ELEMENTS) {
			current_ms = 0;
		}
	}

	globals.use_cond_yield = 0;
	
	for (x = (runtime.microseconds_per_tick / 1000); x <= MAX_ELEMENTS; x += (runtime.microseconds_per_tick / 1000)) {
		if (TIMER_MATRIX[x].mutex && switch_mutex_trylock(TIMER_MATRIX[x].mutex) == SWITCH_STATUS_SUCCESS) {
			switch_thread_cond_broadcast(TIMER_MATRIX[x].cond);
			switch_mutex_unlock(TIMER_MATRIX[x].mutex);
		}
	}

	if (tfd > -1) {
		close(tfd);
		tfd = -1;
	}


	switch_mutex_lock(globals.mutex);
	globals.RUNNING = 0;
	switch_mutex_unlock(globals.mutex);

	switch_delete_profile_timer(&runtime.profile_timer);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Soft timer thread exiting.\n");

	return SWITCH_STATUS_TERM;
}

/* 
   This converts a struct tm to a switch_time_exp_t
   We have to use UNIX structures to do our exams
   and use switch_* functions for the output.
*/

static void tm2switchtime(struct tm *tm, switch_time_exp_t *xt)
{

	if (!xt || !tm) {
		return;
	}
	memset(xt, 0, sizeof(*xt));

	xt->tm_sec = tm->tm_sec;
	xt->tm_min = tm->tm_min;
	xt->tm_hour = tm->tm_hour;
	xt->tm_mday = tm->tm_mday;
	xt->tm_mon = tm->tm_mon;
	xt->tm_year = tm->tm_year;
	xt->tm_wday = tm->tm_wday;
	xt->tm_yday = tm->tm_yday;
	xt->tm_isdst = tm->tm_isdst;

#if defined(HAVE_STRUCT_TM_TM_GMTOFF)
	xt->tm_gmtoff = tm->tm_gmtoff;
#endif

	return;
}

/* **************************************************************************
   LOADING OF THE XML DATA - HASH TABLE & MEMORY POOL MANAGEMENT
   ************************************************************************** */

typedef struct {
	switch_memory_pool_t *pool;
	switch_hash_t *hash;
} switch_timezones_list_t;

static switch_timezones_list_t TIMEZONES_LIST = { 0 };
static switch_event_node_t *NODE = NULL;

SWITCH_DECLARE(const char *) switch_lookup_timezone(const char *tz_name)
{
	char *value = NULL;

	if (tz_name && (value = switch_core_hash_find(TIMEZONES_LIST.hash, tz_name)) == NULL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timezone '%s' not found!\n", tz_name);
	}

	return value;
}

void switch_load_timezones(switch_bool_t reload)
{
	switch_xml_t xml = NULL, x_lists = NULL, x_list = NULL, cfg = NULL;
	unsigned total = 0;

	if (TIMEZONES_LIST.hash) {
		switch_core_hash_destroy(&TIMEZONES_LIST.hash);
	}

	if (TIMEZONES_LIST.pool) {
		switch_core_destroy_memory_pool(&TIMEZONES_LIST.pool);
	}

	memset(&TIMEZONES_LIST, 0, sizeof(TIMEZONES_LIST));
	switch_core_new_memory_pool(&TIMEZONES_LIST.pool);
	switch_core_hash_init(&TIMEZONES_LIST.hash, TIMEZONES_LIST.pool);

	if ((xml = switch_xml_open_cfg("timezones.conf", &cfg, NULL))) {
		if ((x_lists = switch_xml_child(cfg, "timezones"))) {
			for (x_list = switch_xml_child(x_lists, "zone"); x_list; x_list = x_list->next) {
				const char *name = switch_xml_attr(x_list, "name");
				const char *value = switch_xml_attr(x_list, "value");

				if (zstr(name)) {
					continue;
				}

				if (zstr(value)) {
					continue;
				}

				switch_core_hash_insert(TIMEZONES_LIST.hash, name, switch_core_strdup(TIMEZONES_LIST.pool, value));
				total++;
			}
		}

		switch_xml_free(xml);
	}

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "Timezone %sloaded %d definitions\n", reload ? "re" : "", total);
}

static void event_handler(switch_event_t *event)
{
	switch_mutex_lock(globals.mutex);
	switch_load_timezones(1);
	switch_mutex_unlock(globals.mutex);
}

static void tztime(const time_t *const timep, const char *tzstring, struct tm *const tmp);

SWITCH_DECLARE(switch_status_t) switch_time_exp_tz_name(const char *tz, switch_time_exp_t *tm, switch_time_t thetime)
{
	struct tm xtm = { 0 };
	const char *tz_name = tz;
	const char *tzdef;
	time_t timep;

	if (!thetime) {
		thetime = switch_micro_time_now();
	}

	timep = (thetime) / (int64_t) (1000000);

	if (!zstr(tz_name)) {
		tzdef = switch_lookup_timezone(tz_name);
	} else {
		/* We set the default timezone to GMT. */
		tz_name = "GMT";
		tzdef = "GMT";
	}

	if (tzdef) {				/* The lookup of the zone may fail. */
		tztime(&timep, tzdef, &xtm);
		tm2switchtime(&xtm, tm);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;

}

SWITCH_DECLARE(switch_status_t) switch_strftime_tz(const char *tz, const char *format, char *date, size_t len, switch_time_t thetime)
{
	time_t timep;

	const char *tz_name = tz;
	const char *tzdef;

	switch_size_t retsize;

	struct tm tm = { 0 };
	switch_time_exp_t stm;

	if (!thetime) {
		thetime = switch_micro_time_now();
	}

	timep = (thetime) / (int64_t) (1000000);

	if (!zstr(tz_name)) {
		tzdef = switch_lookup_timezone(tz_name);
	} else {
		/* We set the default timezone to GMT. */
		tz_name = "GMT";
		tzdef = "GMT";
	}

	if (tzdef) {				/* The lookup of the zone may fail. */
		tztime(&timep, tzdef, &tm);
		tm2switchtime(&tm, &stm);
		switch_strftime_nocheck(date, &retsize, len, zstr(format) ? "%Y-%m-%d %T" : format, &stm);
		if (!zstr_buf(date)) {
			return SWITCH_STATUS_SUCCESS;
		}
	}
	return SWITCH_STATUS_FALSE;
}

SWITCH_MODULE_LOAD_FUNCTION(softtimer_load)
{
	switch_timer_interface_t *timer_interface;
	module_pool = pool;

#ifdef WIN32
	timeBeginPeriod(1);

	InitializeCriticalSection(&timer_section);

	win32_init_timers(); /* Init timers for Windows, if we should use timeGetTime() or QueryPerformanceCounters() */
#endif

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);

	if ((switch_event_bind_removable(modname, SWITCH_EVENT_RELOADXML, NULL, event_handler, NULL, &NODE) != SWITCH_STATUS_SUCCESS)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't bind!\n");
	}
	switch_load_timezones(0);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	timer_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_TIMER_INTERFACE);
	timer_interface->interface_name = "soft";
	timer_interface->timer_init = timer_init;
	timer_interface->timer_next = timer_next;
	timer_interface->timer_step = timer_step;
	timer_interface->timer_sync = timer_sync;
	timer_interface->timer_check = timer_check;
	timer_interface->timer_destroy = timer_destroy;

	if (!switch_test_flag((&runtime), SCF_USE_CLOCK_RT)) {
		switch_time_set_nanosleep(SWITCH_FALSE);
	}

	if (switch_test_flag((&runtime), SCF_USE_HEAVY_TIMING)) {
		switch_time_set_cond_yield(SWITCH_FALSE);
	}

	if (TFD) {
		switch_clear_flag((&runtime), SCF_CALIBRATE_CLOCK);
	}

#ifdef WIN32
	if (switch_test_flag((&runtime), SCF_USE_WIN32_MONOTONIC)) {
		MONO = 1;

		if (win32_use_qpc) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Enabled Windows monotonic clock, using QueryPerformanceCounter()\n");
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Enabled Windows monotonic clock, using timeGetTime()\n");
		}

		runtime.mono_initiated = switch_mono_micro_time_now(); /* Update mono_initiated, since now is the first time the real clock is enabled */
	}

	/* No need to calibrate clock in Win32, we will only sleep ms anyway, it's just not accurate enough */
	switch_clear_flag((&runtime), SCF_CALIBRATE_CLOCK);
#endif

	if (switch_test_flag((&runtime), SCF_CALIBRATE_CLOCK)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Calibrating timer, please wait...\n");
		switch_time_calibrate_clock();
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CONSOLE, "Clock calibration disabled.\n");
	}

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(softtimer_shutdown)
{
	globals.use_cond_yield = 0;

	if (globals.RUNNING == 1) {
		switch_mutex_lock(globals.mutex);
		globals.RUNNING = -1;
		switch_mutex_unlock(globals.mutex);

		while (globals.RUNNING == -1) {
			do_sleep(10000);
		}
	}
#if defined(WIN32)
	timeEndPeriod(1);
	win32_tick_time_since_start = -1; /* we are not initialized anymore */
	DeleteCriticalSection(&timer_section);
#endif

	if (TIMEZONES_LIST.hash) {
		switch_core_hash_destroy(&TIMEZONES_LIST.hash);
	}

	if (TIMEZONES_LIST.pool) {
		switch_core_destroy_memory_pool(&TIMEZONES_LIST.pool);
	}

	if (NODE) {
		switch_event_unbind(&NODE);
	}

	return SWITCH_STATUS_SUCCESS;
}




/*
 *    This file was originally written for NetBSD and is in the public domain, 
 *    so clarified as of 1996-06-05 by Arthur David Olson (arthur_david_olson@nih.gov).
 *    
 *    Iw was modified by Massimo Cetra in order to be used with Callweaver and Freeswitch.
 */

//#define TESTING_IT 1

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <assert.h>


#ifdef TESTING_IT
#include <sys/time.h>
#endif


#ifndef TRUE
#define TRUE	1
#endif /* !defined TRUE */

#ifndef FALSE
#define FALSE	0
#endif /* !defined FALSE */



#ifndef TZ_MAX_TIMES
/*
** The TZ_MAX_TIMES value below is enough to handle a bit more than a
** year's worth of solar time (corrected daily to the nearest second) or
** 138 years of Pacific Presidential Election time
** (where there are three time zone transitions every fourth year).
*/
#define TZ_MAX_TIMES	370
#endif /* !defined TZ_MAX_TIMES */

#ifndef TZ_MAX_TYPES

#ifndef NOSOLAR
#define TZ_MAX_TYPES	256		/* Limited by what (unsigned char)'s can hold */
#endif /* !defined NOSOLAR */

#ifdef NOSOLAR
/*
** Must be at least 14 for Europe/Riga as of Jan 12 1995,
** as noted by Earl Chew <earl@hpato.aus.hp.com>.
*/
#define TZ_MAX_TYPES	20		/* Maximum number of local time types */
#endif /* !defined NOSOLAR */

#endif /* !defined TZ_MAX_TYPES */

#ifndef TZ_MAX_CHARS
#define TZ_MAX_CHARS	50		/* Maximum number of abbreviation characters */
				/* (limited by what unsigned chars can hold) */
#endif /* !defined TZ_MAX_CHARS */

#ifndef TZ_MAX_LEAPS
#define TZ_MAX_LEAPS	50		/* Maximum number of leap second corrections */
#endif /* !defined TZ_MAX_LEAPS */

#ifdef TZNAME_MAX
#define MY_TZNAME_MAX	TZNAME_MAX
#endif /* defined TZNAME_MAX */

#ifndef TZNAME_MAX
#define MY_TZNAME_MAX	255
#endif /* !defined TZNAME_MAX */


#define SECSPERMIN	60
#define MINSPERHOUR	60
#define HOURSPERDAY	24
#define DAYSPERWEEK	7
#define DAYSPERNYEAR	365
#define DAYSPERLYEAR	366
#define SECSPERHOUR	(SECSPERMIN * MINSPERHOUR)
#define SECSPERDAY	((long) SECSPERHOUR * HOURSPERDAY)
#define MONSPERYEAR	12

#define JULIAN_DAY		0		/* Jn - Julian day */
#define DAY_OF_YEAR		1		/* n - day of year */
#define MONTH_NTH_DAY_OF_WEEK	2	/* Mm.n.d - month, week, day of week */

#define EPOCH_YEAR	1970
#define EPOCH_WDAY	TM_THURSDAY


#ifndef TZ_MAX_TIMES
/*
** The TZ_MAX_TIMES value below is enough to handle a bit more than a
** year's worth of solar time (corrected daily to the nearest second) or
** 138 years of Pacific Presidential Election time
** (where there are three time zone transitions every fourth year).
*/
#define TZ_MAX_TIMES	370
#endif /* !defined TZ_MAX_TIMES */

#ifndef TZDEFRULES
#define TZDEFRULES	"posixrules"
#endif /* !defined TZDEFRULES */

/*
** The DST rules to use if TZ has no rules and we can't load TZDEFRULES.
** We default to US rules as of 1999-08-17.
** POSIX 1003.1 section 8.1.1 says that the default DST rules are
** implementation dependent; for historical reasons, US rules are a
** common default.
*/
#ifndef TZDEFRULESTRING
#define TZDEFRULESTRING ",M4.1.0,M10.5.0"
#endif /* !defined TZDEFDST */

/* Unlike <ctype.h>'s isdigit, this also works if c < 0 | c > UCHAR_MAX.  */
#define is_digit(c) ((unsigned)(c) - '0' <= 9)

#define BIGGEST(a, b)	(((a) > (b)) ? (a) : (b))

#define isleap(y) (((y) % 4) == 0 && (((y) % 100) != 0 || ((y) % 400) == 0))



/*
** INITIALIZE(x)
*/

#ifndef GNUC_or_lint
#ifdef lint
#define GNUC_or_lint
#endif /* defined lint */
#ifndef lint
#ifdef __GNUC__
#define GNUC_or_lint
#endif /* defined __GNUC__ */
#endif /* !defined lint */
#endif /* !defined GNUC_or_lint */
#ifdef WIN32
#define GNUC_or_lint
#endif

#ifndef INITIALIZE
#ifdef GNUC_or_lint
#define INITIALIZE(x)	((x) = 0)
#endif /* defined GNUC_or_lint */
#ifndef GNUC_or_lint
#define INITIALIZE(x)
#endif /* !defined GNUC_or_lint */
#endif /* !defined INITIALIZE */


#define TM_SUNDAY	0
#define TM_MONDAY	1
#define TM_TUESDAY	2
#define TM_WEDNESDAY	3
#define TM_THURSDAY	4
#define TM_FRIDAY	5
#define TM_SATURDAY	6

#define TM_JANUARY	0
#define TM_FEBRUARY	1
#define TM_MARCH	2
#define TM_APRIL	3
#define TM_MAY		4
#define TM_JUNE		5
#define TM_JULY		6
#define TM_AUGUST	7
#define TM_SEPTEMBER	8
#define TM_OCTOBER	9
#define TM_NOVEMBER	10
#define TM_DECEMBER	11

#define TM_YEAR_BASE	1900

#define EPOCH_YEAR	1970
#define EPOCH_WDAY	TM_THURSDAY


/* **************************************************************************
	    
   ************************************************************************** */

static const char gmt[] = "GMT";

#define CHARS_DEF BIGGEST(BIGGEST(TZ_MAX_CHARS + 1, sizeof gmt), (2 * (MY_TZNAME_MAX + 1)))

struct rule {
	int r_type;					/* type of rule--see below */
	int r_day;					/* day number of rule */
	int r_week;					/* week number of rule */
	int r_mon;					/* month number of rule */
	long r_time;				/* transition time of rule */
};

struct ttinfo {					/* time type information */
	long tt_gmtoff;				/* UTC offset in seconds */
	int tt_isdst;				/* used to set tm_isdst */
	int tt_abbrind;				/* abbreviation list index */
	int tt_ttisstd;				/* TRUE if transition is std time */
	int tt_ttisgmt;				/* TRUE if transition is UTC */
};

struct lsinfo {					/* leap second information */
	time_t ls_trans;			/* transition time */
	long ls_corr;				/* correction to apply */
};


struct state {
	int leapcnt;
	int timecnt;
	int typecnt;
	int charcnt;
	time_t ats[TZ_MAX_TIMES];
	unsigned char types[TZ_MAX_TIMES];
	struct ttinfo ttis[TZ_MAX_TYPES];
	char chars[ /* LINTED constant */ CHARS_DEF];
	struct lsinfo lsis[TZ_MAX_LEAPS];
};


static const int mon_lengths[2][MONSPERYEAR] = {
	{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
	{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

static const int year_lengths[2] = {
	DAYSPERNYEAR, DAYSPERLYEAR
};


/* **************************************************************************
	    
   ************************************************************************** */


/*
    Given a pointer into a time zone string, scan until a character that is not
    a valid character in a zone name is found.  Return a pointer to that
    character.
*/

static const char *getzname(register const char *strp)
{
	register char c;

	while ((c = *strp) != '\0' && !is_digit(c) && c != ',' && c != '-' && c != '+')
		++strp;
	return strp;
}


/*
    Given a pointer into a time zone string, extract a number from that string.
    Check that the number is within a specified range; if it is not, return
    NULL.
    Otherwise, return a pointer to the first character not part of the number.
*/

static const char *getnum(register const char *strp, int *const nump, const int min, const int max)
{
	register char c;
	register int num;

	if (strp == NULL || !is_digit(c = *strp))
		return NULL;
	num = 0;
	do {
		num = num * 10 + (c - '0');
		if (num > max)
			return NULL;		/* illegal value */
		c = *++strp;
	} while (is_digit(c));
	if (num < min)
		return NULL;			/* illegal value */
	*nump = num;
	return strp;
}

/*
    Given a pointer into a time zone string, extract a number of seconds,
    in hh[:mm[:ss]] form, from the string.
    If any error occurs, return NULL.
    Otherwise, return a pointer to the first character not part of the number
    of seconds.
*/

static const char *getsecs(register const char *strp, long *const secsp)
{
	int num;

	/*
	 ** `HOURSPERDAY * DAYSPERWEEK - 1' allows quasi-Posix rules like
	 ** "M10.4.6/26", which does not conform to Posix,
	 ** but which specifies the equivalent of
	 ** ``02:00 on the first Sunday on or after 23 Oct''.
	 */
	strp = getnum(strp, &num, 0, HOURSPERDAY * DAYSPERWEEK - 1);
	if (strp == NULL)
		return NULL;
	*secsp = num * (long) SECSPERHOUR;
	if (*strp == ':') {
		++strp;
		strp = getnum(strp, &num, 0, MINSPERHOUR - 1);
		if (strp == NULL)
			return NULL;
		*secsp += num * SECSPERMIN;
		if (*strp == ':') {
			++strp;
			/* `SECSPERMIN' allows for leap seconds.  */
			strp = getnum(strp, &num, 0, SECSPERMIN);
			if (strp == NULL)
				return NULL;
			*secsp += num;
		}
	}
	return strp;
}

/*
    Given a pointer into a time zone string, extract an offset, in
    [+-]hh[:mm[:ss]] form, from the string.
    If any error occurs, return NULL.
    Otherwise, return a pointer to the first character not part of the time.
*/

static const char *getoffset(register const char *strp, long *const offsetp)
{
	register int neg = 0;

	if (*strp == '-') {
		neg = 1;
		++strp;
	} else if (*strp == '+')
		++strp;
	strp = getsecs(strp, offsetp);
	if (strp == NULL)
		return NULL;			/* illegal time */
	if (neg)
		*offsetp = -*offsetp;
	return strp;
}

/*
    Given a pointer into a time zone string, extract a rule in the form
    date[/time].  See POSIX section 8 for the format of "date" and "time".
    If a valid rule is not found, return NULL.
    Otherwise, return a pointer to the first character not part of the rule.
*/

static const char *getrule(const char *strp, register struct rule *const rulep)
{
	if (*strp == 'J') {
		/*
		 ** Julian day.
		 */
		rulep->r_type = JULIAN_DAY;
		++strp;
		strp = getnum(strp, &rulep->r_day, 1, DAYSPERNYEAR);
	} else if (*strp == 'M') {
		/*
		 ** Month, week, day.
		 */
		rulep->r_type = MONTH_NTH_DAY_OF_WEEK;
		++strp;
		strp = getnum(strp, &rulep->r_mon, 1, MONSPERYEAR);
		if (strp == NULL)
			return NULL;
		if (*strp++ != '.')
			return NULL;
		strp = getnum(strp, &rulep->r_week, 1, 5);
		if (strp == NULL)
			return NULL;
		if (*strp++ != '.')
			return NULL;
		strp = getnum(strp, &rulep->r_day, 0, DAYSPERWEEK - 1);
	} else if (is_digit(*strp)) {
		/*
		 ** Day of year.
		 */
		rulep->r_type = DAY_OF_YEAR;
		strp = getnum(strp, &rulep->r_day, 0, DAYSPERLYEAR - 1);
	} else
		return NULL;			/* invalid format */
	if (strp == NULL)
		return NULL;
	if (*strp == '/') {
		/*
		 ** Time specified.
		 */
		++strp;
		strp = getsecs(strp, &rulep->r_time);
	} else
		rulep->r_time = 2 * SECSPERHOUR;	/* default = 2:00:00 */
	return strp;
}


/*
    Given the Epoch-relative time of January 1, 00:00:00 UTC, in a year, the
    year, a rule, and the offset from UTC at the time that rule takes effect,
    calculate the Epoch-relative time that rule takes effect.
*/

static time_t transtime(const time_t janfirst, const int year, register const struct rule *const rulep, const long offset)
{
	register int leapyear;
	register time_t value;
	register int i;
	int d, m1, yy0, yy1, yy2, dow;

	INITIALIZE(value);
	leapyear = isleap(year);
	switch (rulep->r_type) {

	case JULIAN_DAY:
		/*
		 ** Jn - Julian day, 1 == January 1, 60 == March 1 even in leap
		 ** years.
		 ** In non-leap years, or if the day number is 59 or less, just
		 ** add SECSPERDAY times the day number-1 to the time of
		 ** January 1, midnight, to get the day.
		 */
		value = janfirst + (rulep->r_day - 1) * SECSPERDAY;
		if (leapyear && rulep->r_day >= 60)
			value += SECSPERDAY;
		break;

	case DAY_OF_YEAR:
		/*
		 ** n - day of year.
		 ** Just add SECSPERDAY times the day number to the time of
		 ** January 1, midnight, to get the day.
		 */
		value = janfirst + rulep->r_day * SECSPERDAY;
		break;

	case MONTH_NTH_DAY_OF_WEEK:
		/*
		 ** Mm.n.d - nth "dth day" of month m.
		 */
		value = janfirst;
		for (i = 0; i < rulep->r_mon - 1; ++i)
			value += mon_lengths[leapyear][i] * SECSPERDAY;

		/*
		 ** Use Zeller's Congruence to get day-of-week of first day of
		 ** month.
		 */
		m1 = (rulep->r_mon + 9) % 12 + 1;
		yy0 = (rulep->r_mon <= 2) ? (year - 1) : year;
		yy1 = yy0 / 100;
		yy2 = yy0 % 100;
		dow = ((26 * m1 - 2) / 10 + 1 + yy2 + yy2 / 4 + yy1 / 4 - 2 * yy1) % 7;
		if (dow < 0)
			dow += DAYSPERWEEK;

		/*
		 ** "dow" is the day-of-week of the first day of the month.  Get
		 ** the day-of-month (zero-origin) of the first "dow" day of the
		 ** month.
		 */
		d = rulep->r_day - dow;
		if (d < 0)
			d += DAYSPERWEEK;
		for (i = 1; i < rulep->r_week; ++i) {
			if (d + DAYSPERWEEK >= mon_lengths[leapyear][rulep->r_mon - 1])
				break;
			d += DAYSPERWEEK;
		}

		/*
		 ** "d" is the day-of-month (zero-origin) of the day we want.
		 */
		value += d * SECSPERDAY;
		break;
	}

	/*
	 ** "value" is the Epoch-relative time of 00:00:00 UTC on the day in
	 ** question.  To get the Epoch-relative time of the specified local
	 ** time on that day, add the transition time and the current offset
	 ** from UTC.
	 */
	return value + rulep->r_time + offset;
}



/*
    Given a POSIX section 8-style TZ string, fill in the rule tables as
    appropriate.
*/

static int tzparse(const char *name, register struct state *const sp, const int lastditch)
{
	const char *stdname;
	const char *dstname;
	size_t stdlen;
	size_t dstlen;
	long stdoffset;
	long dstoffset;
	register time_t *atp;
	register unsigned char *typep;
	register char *cp;


	INITIALIZE(dstname);
	stdname = name;

	if (lastditch) {
		stdlen = strlen(name);	/* length of standard zone name */
		name += stdlen;
		if (stdlen >= sizeof sp->chars)
			stdlen = (sizeof sp->chars) - 1;
		stdoffset = 0;
	} else {
		name = getzname(name);
		stdlen = name - stdname;
		if (stdlen < 3)
			return -1;
		if (*name == '\0')
			return -1;
		name = getoffset(name, &stdoffset);
		if (name == NULL)
			return -1;
	}

	sp->leapcnt = 0;			/* so, we're off a little */

	if (*name != '\0') {
		dstname = name;
		name = getzname(name);
		dstlen = name - dstname;	/* length of DST zone name */
		if (dstlen < 3)
			return -1;
		if (*name != '\0' && *name != ',' && *name != ';') {
			name = getoffset(name, &dstoffset);
			if (name == NULL)
				return -1;
		} else
			dstoffset = stdoffset - SECSPERHOUR;

		/* Go parsing the daylight saving stuff */
		if (*name == ',' || *name == ';') {
			struct rule start;
			struct rule end;
			register int year;
			register time_t janfirst;
			time_t starttime;
			time_t endtime;

			++name;
			if ((name = getrule(name, &start)) == NULL)
				return -1;
			if (*name++ != ',')
				return -1;
			if ((name = getrule(name, &end)) == NULL)
				return -1;
			if (*name != '\0')
				return -1;

			sp->typecnt = 2;	/* standard time and DST */

			/*
			 ** Two transitions per year, from EPOCH_YEAR to 2037.
			 */
			sp->timecnt = 2 * (2037 - EPOCH_YEAR + 1);

			if (sp->timecnt > TZ_MAX_TIMES)
				return -1;

			sp->ttis[0].tt_gmtoff = -dstoffset;
			sp->ttis[0].tt_isdst = 1;
			sp->ttis[0].tt_abbrind = (int) (stdlen + 1);
			sp->ttis[1].tt_gmtoff = -stdoffset;
			sp->ttis[1].tt_isdst = 0;
			sp->ttis[1].tt_abbrind = 0;

			atp = sp->ats;
			typep = sp->types;
			janfirst = 0;

			for (year = EPOCH_YEAR; year <= 2037; ++year) {
				starttime = transtime(janfirst, year, &start, stdoffset);
				endtime = transtime(janfirst, year, &end, dstoffset);
				if (starttime > endtime) {
					*atp++ = endtime;
					*typep++ = 1;	/* DST ends */
					*atp++ = starttime;
					*typep++ = 0;	/* DST begins */
				} else {
					*atp++ = starttime;
					*typep++ = 0;	/* DST begins */
					*atp++ = endtime;
					*typep++ = 1;	/* DST ends */
				}

				janfirst += year_lengths[isleap(year)] * SECSPERDAY;
			}

		} else {
			register long theirstdoffset;
			register long theirdstoffset;
			register long theiroffset;
			register int isdst;
			register int i;
			register int j;

			if (*name != '\0')
				return -1;
			/*
			   Initial values of theirstdoffset and theirdstoffset.
			 */
			theirstdoffset = 0;
			for (i = 0; i < sp->timecnt; ++i) {
				j = sp->types[i];
				if (!sp->ttis[j].tt_isdst) {
					theirstdoffset = -sp->ttis[j].tt_gmtoff;
					break;
				}
			}
			theirdstoffset = 0;
			for (i = 0; i < sp->timecnt; ++i) {
				j = sp->types[i];
				if (sp->ttis[j].tt_isdst) {
					theirdstoffset = -sp->ttis[j].tt_gmtoff;
					break;
				}
			}
			/*
			 ** Initially we're assumed to be in standard time.
			 */
			isdst = FALSE;
			theiroffset = theirstdoffset;
			/*
			 ** Now juggle transition times and types
			 ** tracking offsets as you do.
			 */
			for (i = 0; i < sp->timecnt; ++i) {
				j = sp->types[i];
				sp->types[i] = (unsigned char) sp->ttis[j].tt_isdst;
				if (sp->ttis[j].tt_ttisgmt) {
					/* No adjustment to transition time */
				} else {
					/*
					 ** If summer time is in effect, and the
					 ** transition time was not specified as
					 ** standard time, add the summer time
					 ** offset to the transition time;
					 ** otherwise, add the standard time
					 ** offset to the transition time.
					 */
					/*
					 ** Transitions from DST to DDST
					 ** will effectively disappear since
					 ** POSIX provides for only one DST
					 ** offset.
					 */
					if (isdst && !sp->ttis[j].tt_ttisstd) {
						sp->ats[i] += dstoffset - theirdstoffset;
					} else {
						sp->ats[i] += stdoffset - theirstdoffset;
					}
				}
				theiroffset = -sp->ttis[j].tt_gmtoff;
				if (sp->ttis[j].tt_isdst)
					theirdstoffset = theiroffset;
				else
					theirstdoffset = theiroffset;
			}
			/*
			 ** Finally, fill in ttis.
			 ** ttisstd and ttisgmt need not be handled.
			 */
			sp->ttis[0].tt_gmtoff = -stdoffset;
			sp->ttis[0].tt_isdst = FALSE;
			sp->ttis[0].tt_abbrind = 0;
			sp->ttis[1].tt_gmtoff = -dstoffset;
			sp->ttis[1].tt_isdst = TRUE;
			sp->ttis[1].tt_abbrind = (int) (stdlen + 1);
			sp->typecnt = 2;
		}
	} else {
		dstlen = 0;
		sp->typecnt = 1;		/* only standard time */
		sp->timecnt = 0;
		sp->ttis[0].tt_gmtoff = -stdoffset;
		sp->ttis[0].tt_isdst = 0;
		sp->ttis[0].tt_abbrind = 0;
	}

	sp->charcnt = (int) (stdlen + 1);
	if (dstlen != 0)
		sp->charcnt += (int) (dstlen + 1);
	if ((size_t) sp->charcnt > sizeof sp->chars)
		return -1;
	cp = sp->chars;
	(void) strncpy(cp, stdname, stdlen);
	cp += stdlen;
	*cp++ = '\0';
	if (dstlen != 0) {
		(void) strncpy(cp, dstname, dstlen);
		*(cp + dstlen) = '\0';
	}
	return 0;
}

/* **************************************************************************
	    
   ************************************************************************** */
#if (_MSC_VER >= 1400)			// VC8+
#define switch_assert(expr) assert(expr);__analysis_assume( expr )
#else
#define switch_assert(expr) assert(expr)
#endif

static void timesub(const time_t *const timep, const long offset, register const struct state *const sp, register struct tm *const tmp)
{
	register const struct lsinfo *lp;
	register long days;
	register time_t rem;
	register int y;
	register int yleap;
	register const int *ip;
	register long corr;
	register int hit;
	register int i;

	switch_assert(timep != NULL);
	switch_assert(sp != NULL);
	switch_assert(tmp != NULL);

	corr = 0;
	hit = 0;
	i = (sp == NULL) ? 0 : sp->leapcnt;

	while (--i >= 0) {
		lp = &sp->lsis[i];
		if (*timep >= lp->ls_trans) {
			if (*timep == lp->ls_trans) {
				hit = ((i == 0 && lp->ls_corr > 0) || (i > 0 && lp->ls_corr > sp->lsis[i - 1].ls_corr));
				if (hit)
					while (i > 0 && sp->lsis[i].ls_trans == sp->lsis[i - 1].ls_trans + 1 && sp->lsis[i].ls_corr == sp->lsis[i - 1].ls_corr + 1) {
						++hit;
						--i;
					}
			}
			corr = lp->ls_corr;
			break;
		}
	}
	days = (long) (*timep / SECSPERDAY);
	rem = *timep % SECSPERDAY;


#ifdef mc68k
	/* If this is for CPU bugs workarounds, i would remove this anyway. Who would use it on an old mc68k ? */
	if (*timep == 0x80000000) {
		/*
		 ** A 3B1 muffs the division on the most negative number.
		 */
		days = -24855;
		rem = -11648;
	}
#endif

	rem += (offset - corr);
	while (rem < 0) {
		rem += SECSPERDAY;
		--days;
	}
	while (rem >= SECSPERDAY) {
		rem -= SECSPERDAY;
		++days;
	}
	tmp->tm_hour = (int) (rem / SECSPERHOUR);
	rem = rem % SECSPERHOUR;
	tmp->tm_min = (int) (rem / SECSPERMIN);

	/*
	 ** A positive leap second requires a special
	 ** representation.  This uses "... ??:59:60" et seq.
	 */
	tmp->tm_sec = (int) (rem % SECSPERMIN) + hit;
	tmp->tm_wday = (int) ((EPOCH_WDAY + days) % DAYSPERWEEK);

	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYSPERWEEK;

	y = EPOCH_YEAR;

#define LEAPS_THRU_END_OF(y)	((y) / 4 - (y) / 100 + (y) / 400)

	while (days < 0 || days >= (long) year_lengths[yleap = isleap(y)]) {
		register int newy;

		newy = (int) (y + days / DAYSPERNYEAR);
		if (days < 0)
			--newy;
		days -= (newy - y) * DAYSPERNYEAR + LEAPS_THRU_END_OF(newy - 1) - LEAPS_THRU_END_OF(y - 1);
		y = newy;
	}

	tmp->tm_year = y - TM_YEAR_BASE;
	tmp->tm_yday = (int) days;

	ip = mon_lengths[yleap];

	for (tmp->tm_mon = 0; days >= (long) ip[tmp->tm_mon]; ++(tmp->tm_mon))
		days = days - (long) ip[tmp->tm_mon];

	tmp->tm_mday = (int) (days + 1);
	tmp->tm_isdst = 0;
#if defined(HAVE_STRUCT_TM_TM_GMTOFF)
	tmp->tm_gmtoff = offset;
#endif
}

/* **************************************************************************
	    
   ************************************************************************** */

static void tztime(const time_t *const timep, const char *tzstring, struct tm *const tmp)
{
	struct state *tzptr, *sp;
	const time_t t = *timep;
	register int i;
	register const struct ttinfo *ttisp;

	if (tzstring == NULL)
		tzstring = gmt;

	tzptr = (struct state *) malloc(sizeof(struct state));
	sp = tzptr;

	if (tzptr != NULL) {

		memset(tzptr, 0, sizeof(struct state));

		(void) tzparse(tzstring, tzptr, FALSE);

		if (sp->timecnt == 0 || t < sp->ats[0]) {
			i = 0;
			while (sp->ttis[i].tt_isdst)
				if (++i >= sp->typecnt) {
					i = 0;
					break;
				}
		} else {
			for (i = 1; i < sp->timecnt; ++i)
				if (t < sp->ats[i])
					break;
			i = sp->types[i - 1];	// DST begin or DST end
		}
		ttisp = &sp->ttis[i];

		/*
		   To get (wrong) behavior that's compatible with System V Release 2.0
		   you'd replace the statement below with
		   t += ttisp->tt_gmtoff;
		   timesub(&t, 0L, sp, tmp);
		 */
		if (tmp != NULL) {		/* Just a check not to assert */
			timesub(&t, ttisp->tt_gmtoff, sp, tmp);
			tmp->tm_isdst = ttisp->tt_isdst;
#if defined(HAVE_STRUCT_TM_TM_ZONE)
			tmp->tm_zone = &sp->chars[ttisp->tt_abbrind];
#endif
		}

		free(tzptr);
	}

}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
