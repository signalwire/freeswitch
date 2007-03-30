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
#include "private/switch_core.h"

struct switch_core_port_allocator {
	switch_port_t start;
	switch_port_t end;
	switch_port_t next;
	uint8_t inc;
	switch_mutex_t *mutex;
	switch_memory_pool_t *pool;
};

SWITCH_DECLARE(switch_status_t) switch_core_port_allocator_new(switch_port_t start,
															   switch_port_t end, uint8_t inc, switch_core_port_allocator_t **new_allocator)
{
	switch_status_t status;
	switch_memory_pool_t *pool;
	switch_core_port_allocator_t *alloc;

	if ((status = switch_core_new_memory_pool(&pool)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	if (!(alloc = switch_core_alloc(pool, sizeof(*alloc)))) {
		switch_core_destroy_memory_pool(&pool);
		return SWITCH_STATUS_MEMERR;
	}

	alloc->start = start;
	alloc->next = start;
	alloc->end = end;
	if (!(alloc->inc = inc)) {
		alloc->inc = 2;
	}
	switch_mutex_init(&alloc->mutex, SWITCH_MUTEX_NESTED, pool);
	alloc->pool = pool;
	*new_allocator = alloc;

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_management_exec(char *relative_oid, switch_management_action_t action, char *data, switch_size_t datalen)
{
	const switch_management_interface_t *ptr;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if ((ptr = switch_loadable_module_get_management_interface(relative_oid))) {
		status = ptr->management_function(relative_oid, action, data, datalen);
	}

	return status;
}



SWITCH_DECLARE(switch_port_t) switch_core_port_allocator_request_port(switch_core_port_allocator_t *alloc)
{
	switch_port_t port;

	switch_mutex_lock(alloc->mutex);
	port = alloc->next;
	alloc->next = alloc->next + alloc->inc;
	if (alloc->next > alloc->end) {
		alloc->next = alloc->start;
	}
	switch_mutex_unlock(alloc->mutex);
	return port;
}

SWITCH_DECLARE(void) switch_core_port_allocator_destroy(switch_core_port_allocator_t **alloc)
{
	switch_memory_pool_t *pool = (*alloc)->pool;
	switch_core_destroy_memory_pool(&pool);
	*alloc = NULL;
}
