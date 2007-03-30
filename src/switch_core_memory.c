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
 * switch_core_memory.c -- Main Core Library (memory management)
 *
 */
#include <switch.h>
#include "private/switch_core.h"

static struct {
	switch_memory_pool_t *memory_pool;
} runtime;

SWITCH_DECLARE(switch_memory_pool_t *) switch_core_session_get_pool(switch_core_session_t *session)
{
	return session->pool;
}

/* **ONLY** alloc things with this function that **WILL NOT** outlive
   the session itself or expect an earth shattering KABOOM!*/
SWITCH_DECLARE(void *) switch_core_session_alloc(switch_core_session_t *session, switch_size_t memory)
{
	void *ptr = NULL;
	assert(session != NULL);
	assert(session->pool != NULL);

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate %d\n", memory);
#endif


	if ((ptr = apr_palloc(session->pool, memory)) != 0) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

/* **ONLY** alloc things with these functions that **WILL NOT** need
   to be freed *EVER* ie this is for *PERMANENT* memory allocation */

SWITCH_DECLARE(void *) switch_core_permanent_alloc(switch_size_t memory)
{
	void *ptr = NULL;
	assert(runtime.memory_pool != NULL);


#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Perm Allocate %d\n", memory);
#endif

	if ((ptr = apr_palloc(runtime.memory_pool, memory)) != 0) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

SWITCH_DECLARE(char *) switch_core_permanent_strdup(const char *todup)
{
	char *duped = NULL;
	switch_size_t len;
	assert(runtime.memory_pool != NULL);

	if (!todup)
		return NULL;

	len = strlen(todup) + 1;

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Perm Allocate %d\n", len);
#endif

	if (todup && (duped = apr_palloc(runtime.memory_pool, len)) != 0) {
		strncpy(duped, todup, len);
	}
	return duped;
}

SWITCH_DECLARE(char *) switch_core_session_sprintf(switch_core_session_t *session, const char *fmt, ...)
{
	va_list ap;
	char *result = NULL;

	assert(session != NULL);
	assert(session->pool != NULL);
	va_start(ap, fmt);

	result = apr_pvsprintf(session->pool, fmt, ap);

	va_end(ap);

	return result;
}

SWITCH_DECLARE(char *) switch_core_sprintf(switch_memory_pool_t *pool, const char *fmt, ...)
{
	va_list ap;
	char *result = NULL;

	assert(pool != NULL);
	va_start(ap, fmt);

	result = apr_pvsprintf(pool, fmt, ap);

	va_end(ap);

	return result;
}


SWITCH_DECLARE(char *) switch_core_session_strdup(switch_core_session_t *session, const char *todup)
{
	char *duped = NULL;
	switch_size_t len;
	assert(session != NULL);
	assert(session->pool != NULL);

	if (!todup) {
		return NULL;
	}
	len = strlen(todup) + 1;

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate %d\n", len);
#endif

	if (todup && (duped = apr_palloc(session->pool, len)) != 0) {
		strncpy(duped, todup, len);
	}
	return duped;
}


SWITCH_DECLARE(char *) switch_core_strdup(switch_memory_pool_t *pool, const char *todup)
{
	char *duped = NULL;
	switch_size_t len;
	assert(pool != NULL);

	if (!todup) {
		return NULL;
	}

	len = strlen(todup) + 1;

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate %d\n", len);
#endif

	if (todup && (duped = apr_palloc(pool, len)) != 0) {
		strncpy(duped, todup, len);
	}
	return duped;
}

SWITCH_DECLARE(switch_status_t) switch_core_new_memory_pool(switch_memory_pool_t **pool)
{

	if ((apr_pool_create(pool, NULL)) != SWITCH_STATUS_SUCCESS) {
		*pool = NULL;
		return SWITCH_STATUS_MEMERR;
	}
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_core_destroy_memory_pool(switch_memory_pool_t **pool)
{
	apr_pool_destroy(*pool);
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(void *) switch_core_alloc(switch_memory_pool_t *pool, switch_size_t memory)
{
	void *ptr = NULL;
	assert(pool != NULL);

#ifdef DEBUG_ALLOC
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Allocate %d\n", memory);
	/* assert(memory < 600000); */
#endif

	if ((ptr = apr_palloc(pool, memory)) != 0) {
		memset(ptr, 0, memory);
	}
	return ptr;
}

SWITCH_DECLARE(switch_memory_pool_t *) switch_core_memory_init(void)
{
	memset(&runtime, 0, sizeof(runtime));

	apr_pool_create(&runtime.memory_pool, NULL);
	return runtime.memory_pool;
}
