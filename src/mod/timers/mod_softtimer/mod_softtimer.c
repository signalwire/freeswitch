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
 * mod_softtimer.c -- Software Timer Module
 *
 */
#include <switch.h>
#include <stdio.h>

static const char modname[] = "mod_softtimer";

#ifdef WIN32
//#define WINTIMER
#endif

struct timer_private {
#ifdef WINTIMER
	LARGE_INTEGER freq;
	LARGE_INTEGER base;
	LARGE_INTEGER now;
#else
	switch_time_t reference;
#endif
};

static switch_status soft_timer_init(switch_timer *timer)
{
	struct timer_private *private;

	private = switch_core_alloc(timer->memory_pool, sizeof(*private));
	timer->private_info = private;

#ifdef WINTIMER
	QueryPerformanceFrequency(&private->freq);
	QueryPerformanceCounter(&private->base);
#else
	private->reference = switch_time_now();
#endif

	return SWITCH_STATUS_SUCCESS;
}

static switch_status soft_timer_next(switch_timer *timer)
{
	struct timer_private *private = timer->private_info;

#ifdef WINTIMER
	private->base.QuadPart += timer->interval * (private->freq.QuadPart / 1000);
	for (;;) {
		QueryPerformanceCounter(&private->now);
		if (private->now.QuadPart >= private->base.QuadPart) {
			break;
		}
		switch_yield(100);
	}
#else
	private->reference += timer->interval * 1000;

	while (switch_time_now() < private->reference) {
		switch_yield(1000);
	}
#endif

	timer->samplecount += timer->samples;

	return SWITCH_STATUS_SUCCESS;
}

static switch_status soft_timer_destroy(switch_timer *timer)
{
	timer->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static const switch_timer_interface soft_timer_interface = {
	/*.interface_name */ "soft",
	/*.timer_init */ soft_timer_init,
	/*.timer_next */ soft_timer_next,
	/*.timer_destroy */ soft_timer_destroy
};

static const switch_loadable_module_interface mod_timers_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ &soft_timer_interface,
	/*.switch_dialplan_interface */ NULL,
	/*.switch_codec_interface */ NULL,
	/*.switch_application_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status) switch_module_load(const switch_loadable_module_interface **interface, char *filename)
{

	/* connect my internal structure to the blank pointer passed to me */
	*interface = &mod_timers_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}
