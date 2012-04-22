/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
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
 * Chris Rienzo <chris@rienzo.net>
 * Timo Teräs <timo.teras@iki.fi> (based on mod_timerfd.c)
 *
 * mod_posix_timer.c -- soft timer implemented with POSIX timers (timer_create/timer_settime/timer_getoverrun)
 *
 */
#include <switch.h>
#include <time.h>
#include <signal.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_posix_timer_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_posix_timer_shutdown);
SWITCH_MODULE_DEFINITION(mod_posix_timer, mod_posix_timer_load, mod_posix_timer_shutdown, NULL);

#define MAX_INTERVAL 2000 /* ms */
#define TIMERS_PER_INTERVAL 4

typedef struct {
	int users;
	timer_t timer;
	switch_size_t tick;
	switch_mutex_t *mutex;
	switch_thread_cond_t *cond;
	int interval;
	int id;
} interval_timer_t;

static struct {
	switch_memory_pool_t *pool;
	int shutdown;
	interval_timer_t interval_timers[MAX_INTERVAL + 1][TIMERS_PER_INTERVAL];
	int next_interval_timer_id[MAX_INTERVAL + 1];
	switch_mutex_t *interval_timers_mutex;
} globals;

/**
 * Notified by POSIX timer of a tick
 */
static void posix_timer_notify(union sigval data)
{
	interval_timer_t *it = (interval_timer_t *)data.sival_ptr;
	switch_mutex_lock(it->mutex);
	if (it->users) {
		it->tick += 1 + timer_getoverrun(it->timer);
		switch_thread_cond_broadcast(it->cond);
	}
	switch_mutex_unlock(it->mutex);

	if (globals.shutdown) {
		switch_mutex_lock(globals.interval_timers_mutex);
		if (it->users) {
			timer_delete(it->timer);
			memset(&it->timer, 0, sizeof(it->timer));
			it->users = 0;
		}
		switch_mutex_unlock(globals.interval_timers_mutex);
	}
}

/**
 * Start a new timer
 */
static switch_status_t posix_timer_start_interval(interval_timer_t *it, int interval)
{
	struct sigevent sigev;
	struct itimerspec val;

	if (globals.shutdown) {
		return SWITCH_STATUS_GENERR;
	}

	if (it->users <= 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "starting %d ms timer #%d\n", it->interval, it->id + 1);
		/* reset */
		it->tick = 0;
		it->users = 0;

		/* reuse, if possible */
		if (it->mutex == NULL) {
			switch_mutex_init(&it->mutex, SWITCH_MUTEX_NESTED, globals.pool);
			switch_thread_cond_create(&it->cond, globals.pool);
		}

		/* create the POSIX timer.  Will notify the posix_timer_notify thread on ticks. */
		memset(&sigev, 0, sizeof(sigev));
		sigev.sigev_notify = SIGEV_THREAD;
		sigev.sigev_notify_function = posix_timer_notify;
		sigev.sigev_value.sival_ptr = (void *)it;
		if (timer_create(CLOCK_MONOTONIC, &sigev, &it->timer) == -1) {
			return SWITCH_STATUS_GENERR;
		}

		/* start the timer to tick at interval */
		memset(&val, 0, sizeof(val));
		val.it_interval.tv_sec = interval / 1000;
		val.it_interval.tv_nsec = (interval % 1000) * 1000000;
		val.it_value.tv_sec = 0;
		val.it_value.tv_nsec = 100000;
		if (timer_settime(it->timer, 0, &val, NULL) == -1) {
			return SWITCH_STATUS_GENERR;
		}
	}

	it->users++;
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Stop a timer
 */
static switch_status_t posix_timer_stop_interval(interval_timer_t *it)
{
	if (it->users > 0) {
		it->users--;
		if (it->users == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "stopping %d ms timer #%d\n", it->interval, it->id + 1);
			switch_mutex_lock(it->mutex);
			timer_delete(it->timer);
			memset(&it->timer, 0, sizeof(it->timer));
			switch_mutex_unlock(it->mutex);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Timer module interface: start a new timer
 * @param timer the timer
 * @return SWITCH_STATUS_SUCCESS if successful otherwise SWITCH_STATUS_GENERR
 */ 
static switch_status_t posix_timer_init(switch_timer_t *timer)
{
	interval_timer_t *it;
	switch_status_t status;
	int interval_timer_id;

	if (timer->interval < 1 || timer->interval > MAX_INTERVAL) {
		return SWITCH_STATUS_GENERR;
	}

	switch_mutex_lock(globals.interval_timers_mutex);
	interval_timer_id = globals.next_interval_timer_id[timer->interval]++;
	if (globals.next_interval_timer_id[timer->interval] >= TIMERS_PER_INTERVAL) {
		globals.next_interval_timer_id[timer->interval] = 0;
	}

	it = &globals.interval_timers[timer->interval][interval_timer_id];
	it->id = interval_timer_id;
	it->interval = timer->interval;
	status = posix_timer_start_interval(it, timer->interval);
	timer->private_info = it;
	switch_mutex_unlock(globals.interval_timers_mutex);

	return status;
}

/**
 * Timer module interface: step the timer
 * @param timer the timer
 * @return SWITCH_STATUS_SUCCESS
 */
static switch_status_t posix_timer_step(switch_timer_t *timer)
{
	timer->tick++;
	timer->samplecount += timer->samples;

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Timer module interface: wait for next tick
 * @param timer the timer
 * @return SWITCH_STATUS_SUCCESS if successful 
 */
static switch_status_t posix_timer_next(switch_timer_t *timer)
{
	interval_timer_t *it = timer->private_info;

	if ((int)(timer->tick - it->tick) < -1) {
		timer->tick = it->tick;
	}
	posix_timer_step(timer);

	switch_mutex_lock(it->mutex);
	while ((int)(timer->tick - it->tick) > 0 && !globals.shutdown) {
		switch_thread_cond_timedwait(it->cond, it->mutex, 20 * 1000);
	}
	switch_mutex_unlock(it->mutex);

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Timer module interface: sync tick count
 * @param timer the timer
 * @return SWITCH_STATUS_SUCCESS
 */
static switch_status_t posix_timer_sync(switch_timer_t *timer)
{
	interval_timer_t *it = timer->private_info;
	timer->tick = it->tick;

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Timer module interface: check if synched
 * @param timer the timer
 * @param step true if timer should be stepped
 * @return SWITCH_STATUS_SUCCESS if synched, SWITCH_STATUS_FALSE otherwise
 */
static switch_status_t posix_timer_check(switch_timer_t *timer, switch_bool_t step)
{
	interval_timer_t *it = timer->private_info;
	int diff = (int)(timer->tick - it->tick);

	if (diff > 0) {
		/* still pending */
		timer->diff = diff;
		return SWITCH_STATUS_FALSE;
	}
	/* timer pending */
	timer->diff = 0;
	if (step) {
		posix_timer_step(timer);
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Timer module interface: destroy a timer
 * @param timer the timer
 * @return SWITCH_STATUS_SUCCESS if successful
 */
static switch_status_t posix_timer_destroy(switch_timer_t *timer)
{
	interval_timer_t *it = timer->private_info;
	switch_status_t status;

	switch_mutex_lock(globals.interval_timers_mutex);
	status = posix_timer_stop_interval(it);
	switch_mutex_unlock(globals.interval_timers_mutex);

	return status;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_posix_timer_load)
{
	switch_timer_interface_t *timer_interface;

	memset(&globals, 0, sizeof(globals));

	globals.pool = pool;
	switch_mutex_init(&globals.interval_timers_mutex, SWITCH_MUTEX_NESTED, globals.pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(globals.pool, modname);
	timer_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_TIMER_INTERFACE);
	timer_interface->interface_name = "posix";
	timer_interface->timer_init = posix_timer_init;
	timer_interface->timer_next = posix_timer_next;
	timer_interface->timer_step = posix_timer_step;
	timer_interface->timer_sync = posix_timer_sync;
	timer_interface->timer_check = posix_timer_check;
	timer_interface->timer_destroy = posix_timer_destroy;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_posix_timer_shutdown)
{
	globals.shutdown = 1;
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
