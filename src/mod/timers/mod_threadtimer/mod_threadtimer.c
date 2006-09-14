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
 * mod_threadtimer.c -- Software Timer Module
 *
 */
#include <switch.h>
#include <stdio.h>

static switch_memory_pool_t *module_pool = NULL;

static struct {
	int32_t RUNNING;
	switch_mutex_t *mutex;
} globals;

static const char modname[] = "mod_threadtimer";
#define MAX_ELEMENTS 1000

struct timer_private {
	uint32_t tick;
	uint32_t reference;
	uint32_t interval;
	switch_mutex_t *mutex;
	struct timer_private *next;
};

typedef struct timer_private timer_private_t;

struct timer_head {
	timer_private_t *private_info;
	switch_mutex_t *mutex;
};
typedef struct timer_head timer_head_t;

static timer_head_t *TIMER_MATRIX[MAX_ELEMENTS+1];

static inline switch_status_t timer_init(switch_timer_t *timer)
{
	timer_private_t *private_info;
	timer_head_t *head;

	if ((private_info = switch_core_alloc(timer->memory_pool, sizeof(*private_info)))) {
		timer->private_info = private_info;
		if (!TIMER_MATRIX[timer->interval]) {
			if (!(TIMER_MATRIX[timer->interval] = switch_core_alloc(module_pool, sizeof(timer_head_t)))) {
				return SWITCH_STATUS_MEMERR;
			}
			switch_mutex_init(&TIMER_MATRIX[timer->interval]->mutex, SWITCH_MUTEX_NESTED, module_pool);
		}

		head = TIMER_MATRIX[timer->interval];

		switch_mutex_init(&private_info->mutex, SWITCH_MUTEX_NESTED, timer->memory_pool);
		private_info->interval = timer->interval;

		switch_mutex_lock(head->mutex);
		private_info->next = head->private_info;
		head->private_info = private_info;
		switch_mutex_unlock(head->mutex);
		
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_MEMERR;
}

#define MAX_TICKS 0xFFFFFF00
#define check_overflow(p) if (p->reference > MAX_TICKS) {\
		switch_mutex_lock(p->mutex);\
		p->reference = p->tick = 0;\
		switch_mutex_unlock(p->mutex);\
	}\


static inline switch_status_t timer_step(switch_timer_t *timer)
{
	timer_private_t *private_info = timer->private_info;

	switch_mutex_lock(private_info->mutex);
	private_info->reference += private_info->interval;
	switch_mutex_unlock(private_info->mutex);

	return SWITCH_STATUS_SUCCESS;
}


static inline switch_status_t timer_next(switch_timer_t *timer)
{
	timer_private_t *private_info = timer->private_info;


	timer_step(timer);
	while (private_info->tick < private_info->reference) {
		switch_yield(1000);
	}
	check_overflow(private_info);
	timer->samplecount += timer->samples;

	return SWITCH_STATUS_SUCCESS;
}

static inline switch_status_t timer_check(switch_timer_t *timer)

{
	timer_private_t *private_info = timer->private_info;
	switch_status_t status;

	switch_mutex_lock(private_info->mutex);
	if (private_info->tick < private_info->reference) {
		status = SWITCH_STATUS_FALSE;
	} else {
		private_info->reference += private_info->interval;
		check_overflow(private_info);
		status = SWITCH_STATUS_SUCCESS;
	}
	switch_mutex_unlock(private_info->mutex);

	return status;
}


static inline switch_status_t timer_destroy(switch_timer_t *timer)
{
	timer_private_t *private_info = timer->private_info;
	timer_head_t *head;
	timer_private_t *ptr, *last = NULL;

	head = TIMER_MATRIX[timer->interval];
	assert(head != NULL);
	assert(private_info != NULL);

	switch_mutex_lock(head->mutex);
	for(ptr = head->private_info; ptr; ptr = ptr->next) {
		if (ptr == private_info) {
			if (last) {
				last->next = private_info->next;
			} else {
				head->private_info = private_info->next;
			}
		}
		last = ptr;
	}
	switch_mutex_unlock(head->mutex);
	
	timer->private_info = NULL;
	return SWITCH_STATUS_SUCCESS;
}

static const switch_timer_interface_t timer_interface = {
	/*.interface_name */ "thread_soft",
	/*.timer_init */ timer_init,
	/*.timer_next */ timer_next,
	/*.timer_step */ timer_step,
	/*.timer_check */ timer_check,
	/*.timer_destroy */ timer_destroy
};

static const switch_loadable_module_interface_t mod_threadtimer_module_interface = {
	/*.module_name */ modname,
	/*.endpoint_interface */ NULL,
	/*.timer_interface */ &timer_interface,
	/*.switch_dialplan_interface */ NULL,
	/*.switch_codec_interface */ NULL,
	/*.switch_application_interface */ NULL
};

SWITCH_MOD_DECLARE(switch_status_t) switch_module_load(const switch_loadable_module_interface_t **module_interface, char *filename)
{

	if (switch_core_new_memory_pool(&module_pool) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "OH OH no pool\n");
		return SWITCH_STATUS_MEMERR;
	}

	/* connect my internal structure to the blank pointer passed to me */
	*module_interface = &mod_threadtimer_module_interface;

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MOD_DECLARE(switch_status_t) switch_module_runtime(void)
{
	switch_time_t reference = switch_time_now();
	uint32_t current_ms = 0;
	timer_private_t *ptr;
	uint32_t x;
	
	memset(&globals, 0, sizeof(globals));
	switch_mutex_init(&globals.mutex, SWITCH_MUTEX_NESTED, module_pool);

	globals.RUNNING = 1;

	while(globals.RUNNING == 1) {
		reference += 1000;
		
		while (switch_time_now() < reference) {
			switch_yield(500);
		}

		current_ms++;

		for (x = 0; x < 1000; x++) {
			int i = x, index;
			if (i == 0) {
				i = 1;
			}
			
			index = (current_ms % i == 0) ? i : 0; 

			if (TIMER_MATRIX[index] && TIMER_MATRIX[index]->private_info) {
				switch_mutex_lock(TIMER_MATRIX[index]->mutex);
				for (ptr = TIMER_MATRIX[index]->private_info; ptr; ptr = ptr->next) {
					switch_mutex_lock(ptr->mutex);
					ptr->tick += ptr->interval;
					switch_mutex_unlock(ptr->mutex);
				}
				switch_mutex_unlock(TIMER_MATRIX[index]->mutex);
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


SWITCH_MOD_DECLARE(switch_status_t) switch_module_shutdown(void)
{
	
	if (globals.RUNNING) {
		switch_mutex_lock(globals.mutex);
		globals.RUNNING = -1;
		switch_mutex_unlock(globals.mutex);
		
		while (globals.RUNNING) {
			switch_yield(10000);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}
