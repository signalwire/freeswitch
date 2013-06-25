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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 *
 * The Initial Developer of the Original Code is
 * Anthony Minessale II <anthm@freeswitch.org>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Timo Teräs <timo.teras@iki.fi>
 *
 *
 * mod_timerfd.c -- Timer implementation using Linux timerfd
 *
 */

#include <switch.h>
#ifdef TIMERFD_WRAP
#define TFD_CLOEXEC 0
#include <timerfd_wrap.h>
#else
#include <sys/timerfd.h>
#endif
#include <sys/epoll.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_timerfd_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_timerfd_shutdown);
SWITCH_MODULE_RUNTIME_FUNCTION(mod_timerfd_runtime);
SWITCH_MODULE_DEFINITION(mod_timerfd, mod_timerfd_load, mod_timerfd_shutdown, mod_timerfd_runtime);

#define MAX_INTERVAL		2000 /* ms */

struct interval_timer {
	int			fd;
	int			users;
	switch_size_t		tick;
	switch_mutex_t		*mutex;
	switch_thread_cond_t	*cond;
};
typedef struct interval_timer interval_timer_t;

static switch_memory_pool_t *module_pool = NULL;
static switch_mutex_t *interval_timers_mutex;
static interval_timer_t interval_timers[MAX_INTERVAL + 1];
static int interval_poll_fd;

static switch_status_t timerfd_start_interval(interval_timer_t *it, int interval)
{
	struct itimerspec val;
	struct epoll_event e;
	int fd;

	it->users++;
	if (it->users > 1)
		return SWITCH_STATUS_SUCCESS;

	it->tick = 0;
	switch_mutex_init(&it->mutex, SWITCH_MUTEX_NESTED, module_pool);
	switch_thread_cond_create(&it->cond, module_pool);

	fd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if (fd < 0)
		return SWITCH_STATUS_GENERR;

	val.it_interval.tv_sec = interval / 1000;
	val.it_interval.tv_nsec = (interval % 1000) * 1000000;
	val.it_value.tv_sec = 0;
	val.it_value.tv_nsec = 100000;

	if (timerfd_settime(fd, 0, &val, NULL) < 0) {
		close(fd);
		return SWITCH_STATUS_GENERR;
	}

	e.events = EPOLLIN | EPOLLERR;
	e.data.ptr = it;
	if (epoll_ctl(interval_poll_fd, EPOLL_CTL_ADD, fd, &e) < 0) {
		close(fd);
		return SWITCH_STATUS_GENERR;
	}

	it->fd = fd;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t timerfd_stop_interval(interval_timer_t *it)
{
	it->users--;
	if (it->users > 0)
		return SWITCH_STATUS_SUCCESS;

	close(it->fd);
	it->fd = -1;
	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t timerfd_init(switch_timer_t *timer)
{
	interval_timer_t *it;
	int rc;

	if (timer->interval < 1 || timer->interval > MAX_INTERVAL)
		return SWITCH_STATUS_GENERR;

	it = &interval_timers[timer->interval];
	switch_mutex_lock(interval_timers_mutex);
	rc = timerfd_start_interval(it, timer->interval);
	timer->private_info = it;
	switch_mutex_unlock(interval_timers_mutex);

	return rc;
}

static switch_status_t timerfd_step(switch_timer_t *timer)
{
	timer->tick++;
	timer->samplecount += timer->samples;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t timerfd_next(switch_timer_t *timer)
{
	interval_timer_t *it = timer->private_info;

	if ((int)(timer->tick - it->tick) < -1)
		timer->tick = it->tick;
	timerfd_step(timer);

	switch_mutex_lock(it->mutex);
	while ((int)(timer->tick - it->tick) > 0)
		switch_thread_cond_wait(it->cond, it->mutex);
	switch_mutex_unlock(it->mutex);

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t timerfd_sync(switch_timer_t *timer)
{
	interval_timer_t *it = timer->private_info;

	timer->tick = it->tick;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t timerfd_check(switch_timer_t *timer, switch_bool_t step)
{
	interval_timer_t *it = timer->private_info;
	int diff = (int)(timer->tick - it->tick);

	if (diff > 0) {
		/* still pending */
		timer->diff = diff;
		return SWITCH_STATUS_FALSE;
	} else {
		/* timer pending */
		timer->diff = 0;
		if (step)
			timerfd_step(timer);
		return SWITCH_STATUS_SUCCESS;
	}
}

static switch_status_t timerfd_destroy(switch_timer_t *timer)
{
	interval_timer_t *it = timer->private_info;
	int rc;

	switch_mutex_lock(interval_timers_mutex);
	rc = timerfd_stop_interval(it);
	switch_mutex_unlock(interval_timers_mutex);

	return rc;
}

SWITCH_MODULE_LOAD_FUNCTION(mod_timerfd_load)
{
	switch_timer_interface_t *timer_interface;

	module_pool = pool;

	interval_poll_fd = epoll_create(16);
	if (interval_poll_fd < 0)
		return SWITCH_STATUS_GENERR;

	switch_mutex_init(&interval_timers_mutex, SWITCH_MUTEX_NESTED, module_pool);
	memset(interval_timers, 0, sizeof(interval_timers));

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);
	timer_interface = switch_loadable_module_create_interface(*module_interface, SWITCH_TIMER_INTERFACE);
	timer_interface->interface_name = "timerfd";
	timer_interface->timer_init = timerfd_init;
	timer_interface->timer_next = timerfd_next;
	timer_interface->timer_step = timerfd_step;
	timer_interface->timer_sync = timerfd_sync;
	timer_interface->timer_check = timerfd_check;
	timer_interface->timer_destroy = timerfd_destroy;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_timerfd_shutdown)
{
	close(interval_poll_fd);

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_RUNTIME_FUNCTION(mod_timerfd_runtime)
{
	struct epoll_event e[16];
	interval_timer_t *it;
	uint64_t u64;
	int i, r;

	do {
		r = epoll_wait(interval_poll_fd, e, sizeof(e) / sizeof(e[0]), 1000);
		if (r < 0)
			break;
		for (i = 0; i < r; i++) {
			it = e[i].data.ptr;
			if ((e[i].events & EPOLLIN) &&
			    read(it->fd, &u64, sizeof(u64)) == sizeof(u64)) {
				switch_mutex_lock(it->mutex);
				it->tick += u64;
				switch_thread_cond_broadcast(it->cond);
				switch_mutex_unlock(it->mutex);
			}
		}
	} while (1);

	return SWITCH_STATUS_TERM;
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
