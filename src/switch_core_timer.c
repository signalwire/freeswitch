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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_timer.c -- Main Core Library (timer interface)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

SWITCH_DECLARE(switch_status_t) switch_core_timer_init(switch_timer_t *timer, const char *timer_name, int interval, int samples,
													   switch_memory_pool_t *pool)
{
	switch_timer_interface_t *timer_interface;
	switch_status_t status;
	memset(timer, 0, sizeof(*timer));
	if ((timer_interface = switch_loadable_module_get_timer_interface(timer_name)) == 0 || !timer_interface->timer_init) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "invalid timer %s!\n", timer_name);
		return SWITCH_STATUS_GENERR;
	}

	timer->interval = interval;
	timer->samples = samples;
	timer->samplecount = samples;
	timer->timer_interface = timer_interface;

	if (pool) {
		timer->memory_pool = pool;
	} else {
		if ((status = switch_core_new_memory_pool(&timer->memory_pool)) != SWITCH_STATUS_SUCCESS) {
			UNPROTECT_INTERFACE(timer->timer_interface);
			return status;
		}
		switch_set_flag(timer, SWITCH_TIMER_FLAG_FREE_POOL);
	}

	return timer->timer_interface->timer_init(timer);
}

SWITCH_DECLARE(switch_status_t) switch_core_timer_next(switch_timer_t *timer)
{
	if (!timer->timer_interface || !timer->timer_interface->timer_next) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not properly configured.\n");
		return SWITCH_STATUS_GENERR;
	}

	if (timer->timer_interface->timer_next(timer) == SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_SUCCESS;
	} else {
		return SWITCH_STATUS_GENERR;
	}

}

SWITCH_DECLARE(switch_status_t) switch_core_timer_step(switch_timer_t *timer)
{
	if (!timer->timer_interface || !timer->timer_interface->timer_step) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not properly configured.\n");
		return SWITCH_STATUS_GENERR;
	}

	return timer->timer_interface->timer_step(timer);
}


SWITCH_DECLARE(switch_status_t) switch_core_timer_sync(switch_timer_t *timer)
{
	if (!timer->timer_interface || !timer->timer_interface->timer_sync) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not properly configured.\n");
		return SWITCH_STATUS_GENERR;
	}

	return timer->timer_interface->timer_sync(timer);
}

SWITCH_DECLARE(switch_status_t) switch_core_timer_check(switch_timer_t *timer, switch_bool_t step)
{
	if (!timer->timer_interface || !timer->timer_interface->timer_check) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not properly configured.\n");
		return SWITCH_STATUS_GENERR;
	}

	return timer->timer_interface->timer_check(timer, step);
}


SWITCH_DECLARE(switch_status_t) switch_core_timer_destroy(switch_timer_t *timer)
{
	if (!timer->timer_interface || !timer->timer_interface->timer_destroy) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer is not properly configured.\n");
		return SWITCH_STATUS_GENERR;
	}

	timer->timer_interface->timer_destroy(timer);
	UNPROTECT_INTERFACE(timer->timer_interface);

	if (switch_test_flag(timer, SWITCH_TIMER_FLAG_FREE_POOL)) {
		switch_core_destroy_memory_pool(&timer->memory_pool);
	}

	memset(timer, 0, sizeof(*timer));

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
