/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2025-2025, Anthony Minessale II <anthm@freeswitch.org>
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
 * Seven Du <dujinfang@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Seven Du <dujinfang@gmail.com>
 *
 * switch_uuidv7.c -- UUIDv7 generation functions
 *
 */

#include <switch.h>
#include "private/switch_uuidv7_pvt.h"
#ifdef __APPLE__
#include <sys/random.h> /* for macOS getentropy() */
#endif
#ifdef _MSC_VER
#include <bcrypt.h> /* for BCryptGenRandom */
#endif

#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
	#define SWITCH_THREAD_LOCAL static _Thread_local
#else
	/* Fallback to compiler-specific or other methods */
	#ifdef _MSC_VER
		#define SWITCH_THREAD_LOCAL __declspec(thread)
	#elif defined(__GNUC__)
		#define SWITCH_THREAD_LOCAL static __thread
	#else
		// #error "Compiler does not support thread-local storage"
		#define SWITCH_THREAD_LOCAL static
		#define SWITCH_THREAD_LOCAL_NOT_SUPPORTED
	#endif
#endif

#ifndef SWITCH_THREAD_LOCAL_NOT_SUPPORTED
SWITCH_THREAD_LOCAL uint8_t uuid_prev[16] = {0};
SWITCH_THREAD_LOCAL uint8_t rand_bytes[10] = {0};
SWITCH_THREAD_LOCAL size_t n_rand_consumed = 10;
#endif

static void switch_getentropy(unsigned char *rand_bytes, size_t n_rand_consumed) {
#ifdef _MSC_VER
	NTSTATUS status = BCryptGenRandom(
		NULL,                          /* Algorithm handle (NULL for system-preferred RNG) */
		rand_bytes,                    /* Buffer to receive random bytes */
		(ULONG)n_rand_consumed,        /* Size of buffer in bytes */
		BCRYPT_USE_SYSTEM_PREFERRED_RNG  /* Flag for system RNG */
	);
	if (status != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error generating random bytes: 0x%lx\n", status);
	}
#else
	if (getentropy(rand_bytes, n_rand_consumed) != 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "getentropy failed");
	}
#endif
}

int uuidv7_new(uint8_t *uuid_out)
{
	int8_t status;
	uint64_t unix_ts_ms = switch_time_now() / 1000;

#ifdef SWITCH_THREAD_LOCAL_NOT_SUPPORTED
	size_t n_rand_consumed = 10;
	uint8_t rand_bytes[10] = {0};
#endif

	switch_getentropy(rand_bytes, n_rand_consumed);
#ifdef SWITCH_THREAD_LOCAL_NOT_SUPPORTED
	status = uuidv7_generate(uuid_out, unix_ts_ms, rand_bytes, NULL);
#else
	status = uuidv7_generate(uuid_prev, unix_ts_ms, rand_bytes, uuid_prev);
#endif
	if (status < 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "uuidv7_generate failed: %d\n", status);
	} else if (status == UUIDV7_STATUS_CLOCK_ROLLBACK) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "uuidv7_generate: clock rollback detected\n");
	}

#ifdef SWITCH_THREAD_LOCAL_NOT_SUPPORTED
	return status;
#else
	n_rand_consumed = uuidv7_status_n_rand_consumed(status);
	memcpy(uuid_out, uuid_prev, 16);

	return status;
#endif
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
