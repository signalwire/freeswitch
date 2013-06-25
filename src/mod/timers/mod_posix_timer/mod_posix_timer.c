/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Christopher M. Rienzo <chris@rienzo.com>
 * Timo Teräs <timo.teras@iki.fi> (based on mod_timerfd.c)
 *
 * Maintainer: Christopher M. Rienzo <chris@rienzo.com>
 *
 * mod_posix_timer.c -- soft timer implemented with POSIX timers (timer_create/timer_settime/timer_getoverrun)
 *
 */
#include <switch.h>
#include <time.h>       /* timer_* */
#include <signal.h>     /* sigaction(), timer_*, etc. */
#include <unistd.h>     /* pipe() */
#include <fcntl.h>      /* fcntl() */
#include <string.h>     /* strerror() */
#include <stdint.h>     /* uint8_t */
#include <errno.h>      /* errno */
#include <sys/select.h> /* select() */
#include <pthread.h>    /* pthread_sigmask() */

SWITCH_MODULE_LOAD_FUNCTION(mod_posix_timer_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_posix_timer_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_posix_timer_runtime);
SWITCH_MODULE_DEFINITION(mod_posix_timer, mod_posix_timer_load, mod_posix_timer_shutdown, mod_posix_timer_runtime);

#define SIG (SIGRTMAX - 1)
#define MAX_INTERVAL 2000 /* ms */
#define TIMERS_PER_INTERVAL 4
#define MAX_ACTIVE_TIMERS 256 /* one byte */

/**
 * Module's internal timer data.
 * Keeps track of how many users are using the timer
 * and the condvar to signal threads waiting on the timer.
 */
typedef struct {
	/** Number of users of this timer */
	int users;
	/** The POSIX timer handle */
	timer_t timer;
	/** Number of ticks */
	switch_size_t tick;
	/** synchronizes access to condvar, users */
	switch_mutex_t *mutex;
	/** condvar for threads waiting on timer */
	switch_thread_cond_t *cond;
	/** The timer period in ms */
	int interval;
	/** Which timer for this interval */
	int num;
	/** The timer's index into the active_interval_timers array */
	int active_id;
} interval_timer_t;

/**
 * Module global data
 */
static struct {
	/** Module memory pool */
	switch_memory_pool_t *pool;
	/** True if module is shutting down */
	int shutdown;
	/** Maps intervals to timers */
	interval_timer_t interval_timers[MAX_INTERVAL + 1][TIMERS_PER_INTERVAL];
	/** Maps IDs to timers */
	interval_timer_t *active_interval_timers[MAX_ACTIVE_TIMERS];
	/** Next timer to assign for a particular interval */ 
	int next_interval_timer_num[MAX_INTERVAL + 1];
	/** Synchronizes access to timer creation / deletion */
	switch_mutex_t *interval_timers_mutex;
	/** Synchronizes access to active timers array */
	switch_mutex_t *active_timers_mutex;
	/** number of active timers */
	int active_timers_count;
	/** self-pipe to notify thread of tick from a signal handler */
	int timer_tick_pipe[2];
} globals;


/**
 * Handle timer signal
 * @param sig the signal
 * @param si the signal information
 * @param cu unused
 */
static void timer_signal_handler(int sig, siginfo_t *si, void *cu)
{
	if (sig == SIG && si->si_code == SI_TIMER) {
		int val = si->si_value.sival_int;
		if (val >= 0 && val <= MAX_ACTIVE_TIMERS) {
			uint8_t active_id = (uint8_t)val;
			/* notify runtime thread that timer identified by active_id has ticked */
			if (write(globals.timer_tick_pipe[1], &active_id, 1) == -1) {
				/* don't actually care about this error- this is only to make the compiler happy */
			}
		}
	}
}

/**
 * Start a new interval timer
 * @param it the timer
 * @param interval the timer interval
 * @return SWITCH_STATUS_SUCCESS if successful
 */
static switch_status_t interval_timer_start(interval_timer_t *it, int interval)
{
	if (globals.shutdown) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "module is shutting down, ignoring request\n");
		return SWITCH_STATUS_GENERR;
	}

	if (it->users <= 0) {
		struct sigevent sigev;
		struct itimerspec val;
		int active_id = -1;
		int i;

		/* find an available id for this timer */
		for (i = 0; i < MAX_ACTIVE_TIMERS && active_id == -1; i++) {
			switch_mutex_lock(globals.active_timers_mutex);
			if(globals.active_interval_timers[i] == NULL) {
				active_id = i;
			}
			switch_mutex_unlock(globals.active_timers_mutex);
		}
		if (active_id == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "no more timers can be created!\n");
			return SWITCH_STATUS_GENERR;
		}
		it->active_id = active_id;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "starting %d ms timer #%d (%d)\n", it->interval, it->num + 1, it->active_id);

		/* reset timer data */
		it->tick = 0;
		it->users = 0;

		/* reuse mutex/condvar */
		if (it->mutex == NULL) {
			switch_mutex_init(&it->mutex, SWITCH_MUTEX_NESTED, globals.pool);
			switch_thread_cond_create(&it->cond, globals.pool);
		}

		/* create the POSIX timer.  Will send SIG on each tick. */
		memset(&sigev, 0, sizeof(sigev));
		sigev.sigev_notify = SIGEV_SIGNAL;
		sigev.sigev_signo = SIG;
		sigev.sigev_value.sival_int = active_id;
		if (timer_create(CLOCK_MONOTONIC, &sigev, &it->timer) == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to create timer: %s\n", strerror(errno));
			return SWITCH_STATUS_GENERR;
		}

		switch_mutex_lock(globals.active_timers_mutex);
		globals.active_interval_timers[it->active_id] = it;
		globals.active_timers_count++;
		switch_mutex_unlock(globals.active_timers_mutex);

		/* start the timer to tick at interval */
		memset(&val, 0, sizeof(val));
		val.it_interval.tv_sec = interval / 1000;
		val.it_interval.tv_nsec = (interval % 1000) * 1000000;
		val.it_value.tv_sec = 0;
		val.it_value.tv_nsec = 100000;
		if (timer_settime(it->timer, 0, &val, NULL) == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "failed to start timer: %s\n", strerror(errno));
			switch_mutex_lock(globals.active_timers_mutex);
			globals.active_interval_timers[it->active_id] = NULL;
			globals.active_timers_count--;
			switch_mutex_unlock(globals.active_timers_mutex);
			return SWITCH_STATUS_GENERR;
		}
	}

	it->users++;
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Delete an interval timer
 * @param it the interval timer
 */
static void interval_timer_delete(interval_timer_t *it)
{
	/* remove from active timers */
	switch_mutex_lock(globals.active_timers_mutex);
	if (globals.active_interval_timers[it->active_id]) {
		globals.active_interval_timers[it->active_id] = NULL;
		globals.active_timers_count--;
	}
	switch_mutex_unlock(globals.active_timers_mutex);

	/* delete the POSIX timer and mark interval timer as destroyed (users == 0) */
	switch_mutex_lock(it->mutex);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "deleting %d ms timer #%d (%d)\n", it->interval, it->num + 1, it->active_id);
	timer_delete(it->timer);
	memset(&it->timer, 0, sizeof(it->timer));
	it->users = 0;
	switch_mutex_unlock(it->mutex);
}

/**
 * Remove a user from interval timer.  Delete if no more users remain.
 * @param it the interval timer
 * @return SWITCH_STATUS_SUCCESS
 */
static switch_status_t interval_timer_stop(interval_timer_t *it)
{
	if (it->users > 0) {
		it->users--;
		if (it->users == 0) {
			interval_timer_delete(it);
		}
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Timer module interface: start a new timer
 * @param timer the timer
 * @return SWITCH_STATUS_SUCCESS if successful otherwise SWITCH_STATUS_GENERR
 */ 
static switch_status_t mod_posix_timer_init(switch_timer_t *timer)
{
	interval_timer_t *it;
	switch_status_t status;
	int interval_timer_num;

	if (timer->interval < 1 || timer->interval > MAX_INTERVAL) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Bad interval: %d\n", timer->interval);
		return SWITCH_STATUS_GENERR;
	}

	switch_mutex_lock(globals.interval_timers_mutex);
	interval_timer_num = globals.next_interval_timer_num[timer->interval]++;
	if (globals.next_interval_timer_num[timer->interval] >= TIMERS_PER_INTERVAL) {
		globals.next_interval_timer_num[timer->interval] = 0;
	}

	it = &globals.interval_timers[timer->interval][interval_timer_num];
	it->num = interval_timer_num;
	it->interval = timer->interval;
	status = interval_timer_start(it, timer->interval);
	timer->private_info = it;
	switch_mutex_unlock(globals.interval_timers_mutex);

	return status;
}

/**
 * Timer module interface: step the timer
 * @param timer the timer
 * @return SWITCH_STATUS_SUCCESS
 */
static switch_status_t mod_posix_timer_step(switch_timer_t *timer)
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
static switch_status_t mod_posix_timer_next(switch_timer_t *timer)
{
	interval_timer_t *it = timer->private_info;

	if ((int)(timer->tick - it->tick) < -1) {
		timer->tick = it->tick;
	}
	mod_posix_timer_step(timer);

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
static switch_status_t mod_posix_timer_sync(switch_timer_t *timer)
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
static switch_status_t mod_posix_timer_check(switch_timer_t *timer, switch_bool_t step)
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
		mod_posix_timer_step(timer);
	}
	return SWITCH_STATUS_SUCCESS;
}

/**
 * Timer module interface: destroy a timer
 * @param timer the timer
 * @return SWITCH_STATUS_SUCCESS if successful
 */
static switch_status_t mod_posix_timer_destroy(switch_timer_t *timer)
{
	interval_timer_t *it = timer->private_info;
	switch_status_t status;

	switch_mutex_lock(globals.interval_timers_mutex);
	status = interval_timer_stop(it);
	switch_mutex_unlock(globals.interval_timers_mutex);

	return status;
}

/**
 * Load the module
 */
SWITCH_MODULE_LOAD_FUNCTION(mod_posix_timer_load)
{
	switch_timer_interface_t *timer_interface;

	memset(&globals, 0, sizeof(globals));
	globals.timer_tick_pipe[0] = -1;
	globals.timer_tick_pipe[1] = -1;

	globals.pool = pool;
	switch_mutex_init(&globals.interval_timers_mutex, SWITCH_MUTEX_NESTED, globals.pool);
	switch_mutex_init(&globals.active_timers_mutex, SWITCH_MUTEX_NESTED, globals.pool);

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(globals.pool, modname);
	timer_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_TIMER_INTERFACE);
	timer_interface->interface_name = "posix";
	timer_interface->timer_init = mod_posix_timer_init;
	timer_interface->timer_next = mod_posix_timer_next;
	timer_interface->timer_step = mod_posix_timer_step;
	timer_interface->timer_sync = mod_posix_timer_sync;
	timer_interface->timer_check = mod_posix_timer_check;
	timer_interface->timer_destroy = mod_posix_timer_destroy;

	/* the pipe allows a signal handler to notify the runtime thread in a async-signal-safe manner */
	if (pipe(globals.timer_tick_pipe) == -1) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to create pipe\n");
		globals.shutdown = 1;
		return SWITCH_STATUS_GENERR;
	}
	fcntl(globals.timer_tick_pipe[0], F_SETFL, O_NONBLOCK);
	fcntl(globals.timer_tick_pipe[1], F_SETFL, O_NONBLOCK);

	{
		struct sigaction sa;
		sigset_t sigmask;

		/* Prevent SIG from annoying FS process.  It will be unblocked in the runtime thread. */
		sigemptyset(&sigmask);
		sigaddset(&sigmask, SIG);
		sigprocmask(SIG_BLOCK, &sigmask, NULL);

		/* set up signal handler */	
		memset(&sa, 0, sizeof(sa));
		sa.sa_flags = SA_SIGINFO | SA_RESTART;
		sa.sa_sigaction = timer_signal_handler;
		sigfillset(&sa.sa_mask);
		if (sigaction(SIG, &sa, NULL) == -1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Failed to set up signal handler: %s\n", strerror(errno));
			globals.shutdown = 1;
			return SWITCH_STATUS_GENERR;
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/**
 * Runtime thread watches for timer ticks sent by signal handler over pipe.  Broadcasts
 * ticks to session threads waiting on timer.
 */
SWITCH_MODULE_RUNTIME_FUNCTION(mod_posix_timer_runtime)
{
	uint8_t active_ids[32];
	sigset_t sigmask;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "runtime thread starting\n");

	/* allow SIG to be delivered to this thread. */
	sigemptyset(&sigmask);
	sigaddset(&sigmask, SIG);
	pthread_sigmask(SIG_UNBLOCK, &sigmask, NULL);

	/* run until module shutdown */
	while (!globals.shutdown) {
		int retval, i;
		fd_set read_fds;
		struct timeval timeout = { 0, 200 * 1000 }; /* 200 ms */

		/* wait for timer tick */
		FD_ZERO(&read_fds);
		FD_SET(globals.timer_tick_pipe[0], &read_fds);
		retval = select(globals.timer_tick_pipe[0] + 1, &read_fds, NULL, NULL, &timeout);
		if (retval == -1) {
			if (errno == EINTR) {
				/* retry */
				continue;
			}
			if (errno == EBADF) {
				/* done */
				break;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error waiting on pipe: %s. Timer thread exiting\n", strerror(errno));
			break;
		} else if (retval == 0) {
			/* retry */
			continue;
		}
		if (!FD_ISSET(globals.timer_tick_pipe[0], &read_fds)) {
			/* retry */
			continue;
		}

		/* which timer ticked? */
		retval = read(globals.timer_tick_pipe[0], &active_ids, 32);
		if (retval == -1) {
			if (errno == EINTR || errno == EAGAIN) {
				/* retry */
				continue;
			}
			if (errno == EBADF) {
				/* done */
				break;
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error reading from pipe: %s. Timer thread exiting\n", strerror(errno));
			break;
		} else if (retval == 0) {
			/* retry */
			continue;
		}

		/* notify threads of timer tick */
		for (i = 0; i < retval; i++) {
			interval_timer_t *it = NULL;

			/* find interval timer */
			switch_mutex_lock(globals.active_timers_mutex);
			it = globals.active_interval_timers[(int)active_ids[i]];
			switch_mutex_unlock(globals.active_timers_mutex);
			if (it == NULL) {
				continue;
			}

			/* send notification */
			switch_mutex_lock(it->mutex);
			if (it->users) {
				it->tick += 1 + timer_getoverrun(it->timer);
				switch_thread_cond_broadcast(it->cond);
			}
			switch_mutex_unlock(it->mutex);
		}
	}

	globals.shutdown = 1;
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "runtime thread finished\n");
	return SWITCH_STATUS_TERM;
}

/**
 * Module shutdown
 */
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_posix_timer_shutdown)
{
	int i;
	globals.shutdown = 1;

	if (globals.timer_tick_pipe[0] > 0) {
		close(globals.timer_tick_pipe[0]);
	}
	if (globals.timer_tick_pipe[1] > 0) {
		close(globals.timer_tick_pipe[1]);
	}

	/* Delete all active timers */
	switch_mutex_lock(globals.interval_timers_mutex);
	for (i = 0; i < MAX_ACTIVE_TIMERS; i++) {
		interval_timer_t *it;
		switch_mutex_lock(globals.active_timers_mutex);
		it = globals.active_interval_timers[i];
		switch_mutex_unlock(globals.active_timers_mutex);
		if (it) {
			interval_timer_delete(it);
		}
	}
	switch_mutex_unlock(globals.interval_timers_mutex);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
