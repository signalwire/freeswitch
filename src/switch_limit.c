/*
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2014, Anthony Minessale II <anthm@freeswitch.org>
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
 * Rupa Schomaker <rupa@rupa.com>
 *
 * switch_limit.c Limit support
 *
 */

#include <switch.h>
#include <switch_module_interfaces.h> /* this is odd VS 2008 Express requires this- include order problem?? */

static switch_limit_interface_t *get_backend(const char *backend) {
	switch_limit_interface_t *limit = NULL;
	
	if (!backend) {
		return NULL;
	}
	
	if (!(limit = switch_loadable_module_get_limit_interface(backend))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Unable to locate limit backend: %s\n", backend);
	}
	
	return limit;
}

static void release_backend(switch_limit_interface_t *limit) {
	if (limit) {
		UNPROTECT_INTERFACE(limit);
	}
}

SWITCH_DECLARE(void) switch_limit_init(switch_memory_pool_t *pool) {
	if (switch_event_reserve_subclass(LIMIT_EVENT_USAGE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register event subclass \"%s\"", LIMIT_EVENT_USAGE);
	}
}


SWITCH_DECLARE(void) switch_limit_fire_event(const char *backend, const char *realm, const char *key, uint32_t usage, uint32_t rate, uint32_t max, uint32_t ratemax)
{
	switch_event_t *event;

	if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, LIMIT_EVENT_USAGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "backend", backend);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "realm", realm);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "key", key);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "usage", "%d", usage);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "rate", "%d", rate);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "max", "%d", max);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "ratemax", "%d", ratemax);
		switch_event_fire(&event);
	}
}

static switch_status_t limit_state_handler(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_state_t state = switch_channel_get_state(channel);
	const char *vval = switch_channel_get_variable(channel, LIMIT_IGNORE_TRANSFER_VARIABLE);
	const char *backendlist = switch_channel_get_variable(channel, LIMIT_BACKEND_VARIABLE);
	
	if (zstr(backendlist)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Unset limit backendlist!\n");
		return SWITCH_STATUS_SUCCESS;
	}

	if (state >= CS_HANGUP || (state == CS_ROUTING && !switch_true(vval))) {
		int argc = 0;
		char *argv[6] = { 0 };
		char *mydata = strdup(backendlist);
		int x;
		
		argc = switch_separate_string(mydata, ',', argv, (sizeof(argv) / sizeof(argv[0])));
		for (x = 0; x < argc; x++) {
			switch_limit_release(argv[x], session, NULL, NULL);
		}
		switch_core_event_hook_remove_state_change(session, limit_state_handler);
		/* Remove limit_backend variable so we register another hook if limit is called again */
		switch_channel_set_variable(channel, LIMIT_BACKEND_VARIABLE, NULL);
		
		free(mydata);
	}
	
	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_limit_incr(const char *backend, switch_core_session_t *session, const char *realm, const char *resource, const int max, const int interval) {
	switch_limit_interface_t *limit = NULL;
	switch_channel_t *channel = NULL;
	int status = SWITCH_STATUS_SUCCESS;
	
	assert(session);

	channel = switch_core_session_get_channel(session);

	/* locate impl, call appropriate func */
	if (!(limit = get_backend(backend))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Limit subsystem %s not found!\n", backend);
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "incr called: %s_%s max:%d, interval:%d\n",
					  realm, resource, max, interval);
	
	if ((status = limit->incr(session, realm, resource, max, interval)) == SWITCH_STATUS_SUCCESS) {
		/* race condition? what if another leg is doing the same thing? */
		const char *existing = switch_channel_get_variable(channel, LIMIT_BACKEND_VARIABLE);
		if (existing) {
			if (!strstr(existing, backend)) {
				switch_channel_set_variable_printf(channel, LIMIT_BACKEND_VARIABLE, "%s,%s", existing, backend);
			}
		} else {
			switch_channel_set_variable(channel, LIMIT_BACKEND_VARIABLE, backend);
			switch_core_event_hook_add_state_change(session, limit_state_handler);
		}
	}
	
	release_backend(limit);
	
end:
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_limit_release(const char *backend, switch_core_session_t *session, const char *realm, const char *resource) {
	switch_limit_interface_t *limit = NULL;
	int status = SWITCH_STATUS_SUCCESS;
	
	/* locate impl, call appropriate func */
	if (!(limit = get_backend(backend))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Limit subsystem %s not found!\n", backend);
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}
	
	status = limit->release(session, realm, resource);
	
end:
	release_backend(limit);
	return status;
}

SWITCH_DECLARE(int) switch_limit_usage(const char *backend, const char *realm, const char *resource, uint32_t *rcount) {
	switch_limit_interface_t *limit = NULL;
	int usage = 0;
	
	/* locate impl, call appropriate func */
	if (!(limit = get_backend(backend))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Limit subsystem %s not found!\n", backend);
		goto end;
	}
	
	usage = limit->usage(realm, resource, rcount);
	
end:
	release_backend(limit);
	return usage;
}

SWITCH_DECLARE(switch_status_t) switch_limit_reset(const char *backend) {
	switch_limit_interface_t *limit = NULL;
	int status = SWITCH_STATUS_SUCCESS;
	
	/* locate impl, call appropriate func */
	if (!(limit = get_backend(backend))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Limit subsystem %s not found!\n", backend);
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}
	
	status = limit->reset();
	
end:
	release_backend(limit);
	return status;
}

SWITCH_DECLARE(switch_status_t) switch_limit_interval_reset(const char *backend, const char *realm, const char *resource) {
	switch_limit_interface_t *limit = NULL;
	int status = SWITCH_STATUS_SUCCESS;
	
	/* locate impl, call appropriate func */
	if (!(limit = get_backend(backend))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Limit subsystem %s not found!\n", backend);
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}
	
	if (!limit->interval_reset) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Limit subsystem %s does not implement interval_reset!\n", backend);
		switch_goto_status(SWITCH_STATUS_GENERR, end);
	}

	status = limit->interval_reset(realm, resource);
	
end:
	release_backend(limit);
	return status;
}

SWITCH_DECLARE(char *) switch_limit_status(const char *backend) {
	switch_limit_interface_t *limit = NULL;
	char *status = NULL;
	
	/* locate impl, call appropriate func */
	if (!(limit = get_backend(backend))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Limit subsystem %s not found!\n", backend);
		switch_goto_status(strdup("-ERR"), end);
	}
	
	status = limit->status();
	
end:
	release_backend(limit);
	return status;
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
