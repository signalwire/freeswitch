/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005/2006, Anthony Minessale II <anthmct@yahoo.com>
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
 * Anthony Minessale II <anthmct@yahoo.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
 * Anthony Minessale II <anthmct@yahoo.com>
 *
 *
 * softtimer.c -- Software Timer Module
 *
 */

#include <switch.h>
#include <stdio.h>
#include "private/switch_core_pvt.h"

#ifndef UINT32_MAX
#define UINT32_MAX 0xffffffff
#endif

#define MAX_TICK UINT32_MAX - 1024
#define MS_PER_TICK 10
static switch_memory_pool_t *module_pool = NULL;

static struct {
	int32_t RUNNING;
	int32_t STARTED;
	switch_mutex_t *mutex;
} globals;

#ifdef WIN32
#undef SWITCH_MOD_DECLARE_DATA
#define SWITCH_MOD_DECLARE_DATA __declspec(dllexport)
#endif

SWITCH_MODULE_LOAD_FUNCTION(softtimer_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(softtimer_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(softtimer_runtime);
SWITCH_MODULE_DEFINITION(CORE_SOFTTIMER_MODULE, softtimer_load, softtimer_shutdown, softtimer_runtime);

#define MAX_ELEMENTS 3600
#define IDLE_SPEED 100
#define STEP_MS 1
#define STEP_MIC 1000

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
};
typedef struct timer_matrix timer_matrix_t;

static timer_matrix_t TIMER_MATRIX[MAX_ELEMENTS + 1];

SWITCH_DECLARE(switch_time_t) switch_timestamp_now(void)
{
	return runtime.timestamp ? runtime.timestamp : switch_time_now();
}


SWITCH_DECLARE(time_t) switch_timestamp(time_t *t)
{
	time_t now = switch_timestamp_now() / APR_USEC_PER_SEC;
	if (t) {
		*t = now;
	}
	return now;
}

#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
static int MONO = 1;
#else
static int MONO = 0;
#endif


SWITCH_DECLARE(void) switch_time_set_monotonic(switch_bool_t enable)
{
	MONO = enable ? 1 : 0;
	switch_time_sync();
}

static switch_time_t time_now(int64_t offset)
{
	switch_time_t now;

#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
	if (MONO) {
		struct timespec ts;
		clock_gettime(CLOCK_MONOTONIC, &ts);
		now = ts.tv_sec * APR_USEC_PER_SEC + (ts.tv_nsec / 1000) + offset;
	} else {
#endif
		now = switch_time_now();

#if defined(HAVE_CLOCK_GETTIME) && defined(CLOCK_MONOTONIC)
	}
#endif

	return now;
}

SWITCH_DECLARE(void) switch_time_sync(void)
{
	runtime.reference = switch_time_now();
	runtime.offset = runtime.reference - time_now(0);
	runtime.reference = time_now(runtime.offset);
}

SWITCH_DECLARE(void) switch_sleep(switch_interval_time_t t)
{

#if defined(HAVE_CLOCK_NANOSLEEP) && defined(SWITCH_USE_CLOCK_FUNCS)
	struct timespec ts;
	ts.tv_sec = t / APR_USEC_PER_SEC;
	ts.tv_nsec = (t % APR_USEC_PER_SEC) * 1000;

	clock_nanosleep(CLOCK_REALTIME, 0, &ts, NULL);

#elif defined(HAVE_USLEEP)
	usleep(t);
#elif defined(WIN32)
	Sleep((DWORD) ((t) / 1000));
#else
	apr_sleep(t);
#endif


}

static switch_status_t timer_init(switch_timer_t *timer)
{
	timer_private_t *private_info;
	int sanity = 0;

	while (globals.STARTED == 0) {
		switch_yield(100000);
		if (++sanity == 10) {
			break;
		}
	}

	if (globals.RUNNING != 1 || !globals.mutex) {
		return SWITCH_STATUS_FALSE;
	}

	if ((private_info = switch_core_alloc(timer->memory_pool, sizeof(*private_info)))) {
#if defined(WIN32)
		timeBeginPeriod(1);
#endif
		switch_mutex_lock(globals.mutex);
		TIMER_MATRIX[timer->interval].count++;
		switch_mutex_unlock(globals.mutex);
		timer->private_info = private_info;
		private_info->start = private_info->reference = TIMER_MATRIX[timer->interval].tick;
		private_info->roll = TIMER_MATRIX[timer->interval].roll;
		private_info->ready = 1;
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

#define check_roll() if (private_info->roll < TIMER_MATRIX[timer->interval].roll) {	\
		private_info->roll++;											\
		private_info->reference = private_info->start = TIMER_MATRIX[timer->interval].tick;	\
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
		private_info->start = private_info->reference;
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
	if (timer_step(timer) == SWITCH_STATUS_SUCCESS) {
		/* push the reference into the future 2 more intervals to prevent collision */
		private_info->reference += 2;
	}

	return SWITCH_STATUS_SUCCESS;
}


static switch_status_t timer_next(switch_timer_t *timer)
{
	timer_private_t *private_info = timer->private_info;

	timer_step(timer);

	while (globals.RUNNING == 1 && private_info->ready && TIMER_MATRIX[timer->interval].tick < private_info->reference) {
		check_roll();
		switch_yield(1000);
	}

	if (globals.RUNNING == 1) {
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
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
	switch_mutex_lock(globals.mutex);
	TIMER_MATRIX[timer->interval].count--;
	if (TIMER_MATRIX[timer->interval].count == 0) {
		TIMER_MATRIX[timer->interval].tick = 0;
	}
	switch_mutex_unlock(globals.mutex);
	private_info->ready = 0;
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(softtimer_runtime)
{
	switch_time_t too_late = STEP_MIC * 1000;
	uint32_t current_ms = 0;
	uint32_t x, tick = 0;
	switch_time_t ts = 0, last = 0;
	int fwd_errs = 0, rev_errs = 0;

	switch_time_sync();

	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);

	globals.STARTED = globals.RUNNING = 1;
	switch_mutex_lock(runtime.throttle_mutex);
	runtime.sps = runtime.sps_total;
	switch_mutex_unlock(runtime.throttle_mutex);

	if (MONO) {
		int loops;
		for (loops = 0; loops < 3; loops++) {
			ts = time_now(0);
			/* if it returns the same value every time it won't be of much use. */
			if (ts == last) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Broken MONOTONIC Clock Detected!, Support Disabled.\n");
				MONO = 0;
				runtime.reference = switch_time_now();
				runtime.initiated = runtime.reference;
				break;
			}
			switch_yield(STEP_MIC);
			last = ts;
		}
	}

	ts = 0;
	last = 0;
	fwd_errs = rev_errs = 0;

	while (globals.RUNNING == 1) {
		runtime.reference += STEP_MIC;
		while ((ts = time_now(runtime.offset)) < runtime.reference) {
			if (ts < last) {
				if (MONO) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Virtual Migration Detected! Syncing Clock\n");
					switch_time_sync();
				} else {
					int64_t diff = (int64_t) (ts - last);
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Reverse Clock Skew Detected!\n");
					runtime.reference = switch_time_now();
					current_ms = 0;
					tick = 0;
					runtime.initiated += diff;
					rev_errs++;
				}
			} else {
				rev_errs = 0;
			}
			switch_yield(STEP_MIC);
			last = ts;
		}


		if (ts > (runtime.reference + too_late)) {
			if (MONO) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Virtual Migration Detected! Syncing Clock\n");
				switch_time_sync();
			} else {
				switch_time_t diff = ts - runtime.reference - STEP_MIC;
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
			fwd_errs = rev_errs = 0;
		}

		runtime.timestamp = ts;
		current_ms += STEP_MS;
		tick += STEP_MS;

		if (tick >= 1000) {
			if (runtime.sps <= 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Over Session Rate of %d!\n", runtime.sps_total);
			}
			switch_mutex_lock(runtime.throttle_mutex);
			runtime.sps_last = runtime.sps_total - runtime.sps;
			runtime.sps = runtime.sps_total;
			switch_mutex_unlock(runtime.throttle_mutex);
			tick = 0;
		}

		if ((current_ms % MS_PER_TICK) == 0) {
			for (x = MS_PER_TICK; x <= MAX_ELEMENTS; x += MS_PER_TICK) {
				if ((current_ms % x) == 0) {
					if (TIMER_MATRIX[x].count) {
						TIMER_MATRIX[x].tick++;
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

	switch_mutex_lock(globals.mutex);
	globals.RUNNING = 0;
	switch_mutex_unlock(globals.mutex);

	return SWITCH_STATUS_TERM;
}

SWITCH_MODULE_LOAD_FUNCTION(softtimer_load)
{
	switch_timer_interface_t *timer_interface;
	module_pool = pool;

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

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(softtimer_shutdown)
{

	if (globals.RUNNING) {
		switch_mutex_lock(globals.mutex);
		globals.RUNNING = -1;
		switch_mutex_unlock(globals.mutex);

		while (globals.RUNNING) {
			switch_yield(10000);
		}
	}
#if defined(WIN32)
	timeEndPeriod(1);
#endif


	return SWITCH_STATUS_SUCCESS;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 expandtab:
 */
