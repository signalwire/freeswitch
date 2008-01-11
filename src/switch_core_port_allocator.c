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
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_port_allocator.c -- Main Core Library (port allocator)
 *
 */
#include <switch.h>
#include "private/switch_core_pvt.h"

struct switch_core_port_allocator {
	switch_port_t start;
	switch_port_t end;
	switch_port_t next;
	switch_byte_t *track;
	uint32_t track_len;
	uint32_t track_used;
	switch_port_flag_t flags;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
};

SWITCH_DECLARE(switch_status_t) switch_core_port_allocator_new(switch_port_t start,
															   switch_port_t end, switch_port_flag_t flags, switch_core_port_allocator_t **new_allocator)
{
	switch_status_t status;
	switch_memory_pool_t *pool;
	switch_core_port_allocator_t *alloc;
	int even, odd;
	
	if ((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (!(alloc = switch_core_alloc(pool, sizeof(*alloc)))) {
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}
	
	alloc->flags = flags;
	even = switch_test_flag(alloc, SPF_EVEN);
	odd = switch_test_flag(alloc, SPF_ODD);

	if (!(even && odd)) {
		if (even) {
			if ((start % 2) != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Rounding odd start port %d to %d\n", start, start + 1);
				start++;
			}
			if ((end % 2) != 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Rounding odd end port %d to %d\n", end, end - 1);
				end--;
			}
		} else if (odd) {
			if ((start % 2) == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Rounding even start port %d to %d\n", start, start + 1);
				start++;
			}
			if ((end % 2) == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Rounding even end port %d to %d\n", end, end - 1);
				end--;
			}
		}
	}

	alloc->track_len = (end - start) + 2;	

	if (!(even && odd)) {
		alloc->track_len /= 2;
	}
	
	alloc->track = switch_core_alloc(pool, (alloc->track_len + 2) * sizeof(switch_byte_t));
	
	alloc->start = start;
	alloc->next = start;
	alloc->end = end;


	switch_mutex_init(&alloc->mutex, SWITCH_MUTEX_NESTED, pool);
	alloc->pool = pool;
	*new_allocator = alloc;

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_core_port_allocator_request_port(switch_core_port_allocator_t *alloc, switch_port_t *port_ptr)
{
	switch_port_t port = 0;
	switch_status_t status = SWITCH_STATUS_FALSE;
	int even = switch_test_flag(alloc, SPF_EVEN);
	int odd = switch_test_flag(alloc, SPF_ODD);

	switch_mutex_lock(alloc->mutex);
	srand(getpid() + (unsigned)switch_timestamp(NULL));
	
	while(alloc->track_used < alloc->track_len) {
		double r;
		uint32_t index;
		int tries = 0;

		do {
			r = ((double)rand() / ((double)(RAND_MAX)+(double)(1)));
			index = (int) (r * alloc->track_len);
			tries++;
		} while((alloc->track[index] || index >= alloc->track_len) && tries < 10000);
		
		while(alloc->track[index]) {
			if (++index >= alloc->track_len) {
				index = 0;
			}
		}

		if (index < alloc->track_len) {
			alloc->track[index] = 1;
			alloc->track_used++;
			status = SWITCH_STATUS_SUCCESS;

			if ((even && odd)) {
				port = (switch_port_t)(index + alloc->start);
			} else {
				port = (switch_port_t)(index + (alloc->start / 2));
				port *= 2;
			} 
			goto end;
		}
	}
	
	
 end:

	switch_mutex_unlock(alloc->mutex);
	
	if (status == SWITCH_STATUS_SUCCESS) {
		*port_ptr = port;
	} else {
		*port_ptr = 0;
	}

	
	return status;
	
}


SWITCH_DECLARE(switch_status_t) switch_core_port_allocator_free_port(switch_core_port_allocator_t *alloc, switch_port_t port)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	int even = switch_test_flag(alloc, SPF_EVEN);
	int odd = switch_test_flag(alloc, SPF_ODD);
	int index = port - alloc->start;
	
	if (!(even && odd)) {
		index /= 2;
	}

	switch_mutex_lock(alloc->mutex);
	if (alloc->track[index]) {
		alloc->track[index] = 0;
		alloc->track_used--;
		status = SWITCH_STATUS_SUCCESS;
	}
	switch_mutex_unlock(alloc->mutex);

	return status;
}


SWITCH_DECLARE(void) switch_core_port_allocator_destroy(switch_core_port_allocator_t **alloc)
{
	switch_memory_pool_t *pool = (*alloc)->pool;
	switch_core_destroy_memory_pool(&pool);
	*alloc = NULL;
}
