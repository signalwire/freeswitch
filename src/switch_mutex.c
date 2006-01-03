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
 * switch_mutex.c -- Mutex Locking
 *
 */
#include <switch_mutex.h>

SWITCH_DECLARE(switch_status) switch_mutex_init(switch_mutex_t **lock,
												switch_lock_flag flags,
												switch_memory_pool *pool)
{

	return (apr_thread_mutex_create(lock, flags, pool) == APR_SUCCESS) ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_GENERR;
}

SWITCH_DECLARE(switch_status) switch_mutex_destroy(switch_mutex_t *lock)
{
	return apr_thread_mutex_destroy(lock);
}

SWITCH_DECLARE(switch_status) switch_mutex_lock(switch_mutex_t *lock)
{
	return apr_thread_mutex_lock(lock);
}

SWITCH_DECLARE(switch_status) switch_mutex_unlock(switch_mutex_t *lock)
{
	return apr_thread_mutex_unlock(lock);
}

SWITCH_DECLARE(switch_status) switch_mutex_trylock(switch_mutex_t *lock)
{
	return apr_thread_mutex_trylock(lock);
}


