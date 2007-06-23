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
 * switch_core_state_maching.c -- Main Core Library (state machine)
 *
 */
#include <switch.h>
#include "private/switch_core_pvt.h"

static void switch_core_standard_on_init(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard INIT %s\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_hangup(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard HANGUP %s, cause: %s\n",
					  switch_channel_get_name(session->channel), switch_channel_cause2str(switch_channel_get_cause(session->channel)));

}

static void switch_core_standard_on_ring(switch_core_session_t *session)
{
	switch_dialplan_interface_t *dialplan_interface = NULL;
	switch_caller_profile_t *caller_profile;
	switch_caller_extension_t *extension = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard RING %s\n", switch_channel_get_name(session->channel));

	if ((caller_profile = switch_channel_get_caller_profile(session->channel)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't get profile!\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return;
	} else {
		char *dp[25];
		char *dpstr;
		int argc, x, count = 0;

		if (!switch_strlen_zero(caller_profile->dialplan)) {
			if ((dpstr = switch_core_session_strdup(session, caller_profile->dialplan))) {
				argc = switch_separate_string(dpstr, ',', dp, (sizeof(dp) / sizeof(dp[0])));
				for (x = 0; x < argc; x++) {
					char *dpname = dp[x];
					char *dparg = NULL;

					if (dpname) {
						if ((dparg = strchr(dpname, ':'))) {
							*dparg++ = '\0';
						}
					}
					if (!(dialplan_interface = switch_loadable_module_get_dialplan_interface(dpname))) {
						continue;
					}

					count++;

					if ((extension = dialplan_interface->hunt_function(session, dparg, NULL)) != 0) {
						switch_channel_set_caller_extension(session->channel, extension);
						return;
					}
				}
			}
		}

		if (!count) {
			if (switch_channel_test_flag(session->channel, CF_OUTBOUND)) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No Dialplan, changing state to HOLD\n");
				switch_channel_set_state(session->channel, CS_HOLD);
				return;
			}
		}
	}

	if (!extension) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_INFO, "No Route, Aborting\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
	}
}

static void switch_core_standard_on_execute(switch_core_session_t *session)
{
	switch_caller_extension_t *extension;
	const switch_application_interface_t *application_interface;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard EXECUTE\n");

	if ((extension = switch_channel_get_caller_extension(session->channel)) == 0) {
		switch_channel_hangup(session->channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return;
	}

	while (switch_channel_get_state(session->channel) == CS_EXECUTE && extension->current_application) {
		char *expanded = NULL;

		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Execute %s(%s)\n",
						  extension->current_application->application_name, switch_str_nil(extension->current_application->application_data));
		if ((application_interface = switch_loadable_module_get_application_interface(extension->current_application->application_name)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Application %s\n", extension->current_application->application_name);
			switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}

		if (switch_channel_test_flag(session->channel, CF_BYPASS_MEDIA) && !switch_test_flag(application_interface, SAF_SUPPORT_NOMEDIA)) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Application %s Cannot be used with NO_MEDIA mode!\n",
							  extension->current_application->application_name);
			switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}

		if (!application_interface->application_function) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Function for %s\n", extension->current_application->application_name);
			switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}

		if ((expanded =
			 switch_channel_expand_variables(session->channel,
											 extension->current_application->application_data)) != extension->current_application->application_data) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Expanded String %s(%s)\n", extension->current_application->application_name,
							  expanded);
		}

		if (switch_channel_get_variable(session->channel, "presence_id")) {
			char *arg = switch_mprintf("%s(%s)", extension->current_application->application_name, expanded);
			if (arg) {
				switch_channel_presence(session->channel, "unknown", arg);
				switch_safe_free(arg);
			}
		}
		switch_core_session_exec(session, application_interface, expanded);
		//application_interface->application_function(session, expanded);

		if (expanded != extension->current_application->application_data) {
			switch_safe_free(expanded);
		}
		extension->current_application = extension->current_application->next;
	}

	if (switch_channel_get_state(session->channel) == CS_EXECUTE) {
		switch_channel_hangup(session->channel, SWITCH_CAUSE_NORMAL_CLEARING);
	}
}

static void switch_core_standard_on_loopback(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard LOOPBACK\n");
	switch_ivr_session_echo(session);
}

static void switch_core_standard_on_transmit(switch_core_session_t *session)
{
	assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard TRANSMIT\n");
}

static void switch_core_standard_on_hold(switch_core_session_t *session)
{
	assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard HOLD\n");
}

static void switch_core_standard_on_hibernate(switch_core_session_t *session)
{
	assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard HIBERNATE\n");
}

static switch_hash_t *stack_table = NULL;
#if defined (__GNUC__) && defined (LINUX)
#include <execinfo.h>
#include <stdio.h>
#include <stdlib.h>
#define STACK_LEN 10

/* Obtain a backtrace and print it to stdout. */
static void print_trace(void)
{
	void *array[STACK_LEN];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace(array, STACK_LEN);
	strings = backtrace_symbols(array, size);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Obtained %zd stack frames.\n", size);

	for (i = 0; i < size; i++) {
		switch_log_printf(SWITCH_CHANNEL_LOG_CLEAN, SWITCH_LOG_CRIT, "%s\n", strings[i]);
	}

	free(strings);
}
#else
static void print_trace(void)
{
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Trace not avaliable =(\n");
}
#endif


static void handle_fatality(int sig)
{
	switch_thread_id_t thread_id;
	jmp_buf *env;

	if (sig && (thread_id = switch_thread_self())
		&& (env = (jmp_buf *) apr_hash_get(stack_table, &thread_id, sizeof(thread_id)))) {
		print_trace();
		longjmp(*env, sig);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Caught signal %d for unmapped thread!", sig);
		abort();
	}
}


void switch_core_state_machine_init(switch_memory_pool_t *pool)
{
	if (runtime.crash_prot) {
		switch_core_hash_init(&stack_table, pool);
	}
}

SWITCH_DECLARE(void) switch_core_session_run(switch_core_session_t *session)
{
	switch_channel_state_t state = CS_NEW, laststate = CS_HANGUP, midstate = CS_DONE, endstate;
	const switch_endpoint_interface_t *endpoint_interface;
	const switch_state_handler_table_t *driver_state_handler = NULL;
	const switch_state_handler_table_t *application_state_handler = NULL;
	switch_thread_id_t thread_id;
	jmp_buf env;
	int sig;

	if (runtime.crash_prot) {
		thread_id = switch_thread_self();
		signal(SIGSEGV, handle_fatality);
		signal(SIGFPE, handle_fatality);
#ifndef WIN32
		signal(SIGBUS, handle_fatality);
#endif

		if ((sig = setjmp(env)) != 0) {
			switch_event_t *event;

			if (switch_event_create(&event, SWITCH_EVENT_SESSION_CRASH) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(session->channel, event);
				switch_event_fire(&event);
			}
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Thread has crashed for channel %s\n", switch_channel_get_name(session->channel));
			switch_channel_hangup(session->channel, SWITCH_CAUSE_CRASH);
		} else {
			apr_hash_set(stack_table, &thread_id, sizeof(thread_id), &env);
		}
	}


	/*
	   Life of the channel. you have channel and pool in your session
	   everywhere you go you use the session to malloc with
	   switch_core_session_alloc(session, <size>)

	   The enpoint module gets the first crack at implementing the state
	   if it wants to, it can cancel the default behaviour by returning SWITCH_STATUS_FALSE

	   Next comes the channel's event handler table that can be set by an application
	   which also can veto the next behaviour in line by returning SWITCH_STATUS_FALSE

	   Finally the default state behaviour is called.


	 */
	assert(session != NULL);

	session->thread_running = 1;
	endpoint_interface = session->endpoint_interface;
	assert(endpoint_interface != NULL);

	driver_state_handler = endpoint_interface->state_handler;
	assert(driver_state_handler != NULL);

	switch_mutex_lock(session->mutex);

	while ((state = switch_channel_get_state(session->channel)) != CS_DONE) {
		uint8_t exception = 0;
		if (switch_channel_test_flag(session->channel, CF_REPEAT_STATE)) {
			switch_channel_clear_flag(session->channel, CF_REPEAT_STATE);
			exception = 1;
		}
		if (state != laststate || state == CS_HANGUP || exception) {
			int index = 0;
			int proceed = 1;
			midstate = state;

			switch (state) {
			case CS_NEW:		/* Just created, Waiting for first instructions */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State NEW\n", switch_channel_get_name(session->channel));
				break;
			case CS_DONE:
				goto done;
			case CS_HANGUP:	/* Deactivate and end the thread */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State HANGUP\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_hangup
					|| (driver_state_handler->on_hangup
						&& driver_state_handler->on_hangup(session) == SWITCH_STATUS_SUCCESS && midstate == switch_channel_get_state(session->channel))) {
					while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hangup
							|| (application_state_handler->on_hangup
								&& application_state_handler->on_hangup(session) == SWITCH_STATUS_SUCCESS
								&& midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hangup ||
							(application_state_handler->on_hangup &&
							 application_state_handler->on_hangup(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}

					if (proceed) {
						switch_core_standard_on_hangup(session);
					}
				}
				goto done;
			case CS_INIT:		/* Basic setup tasks */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State INIT\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_init
					|| (driver_state_handler->on_init && driver_state_handler->on_init(session) == SWITCH_STATUS_SUCCESS
						&& midstate == switch_channel_get_state(session->channel))) {
					while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_init
							|| (application_state_handler->on_init
								&& application_state_handler->on_init(session) == SWITCH_STATUS_SUCCESS
								&& midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_init ||
							(application_state_handler->on_init &&
							 application_state_handler->on_init(session) == SWITCH_STATUS_SUCCESS
							 && midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_init(session);
					}
				}
				break;
			case CS_RING:		/* Look for a dialplan and find something to do */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State RING\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_ring
					|| (driver_state_handler->on_ring && driver_state_handler->on_ring(session) == SWITCH_STATUS_SUCCESS
						&& midstate == switch_channel_get_state(session->channel))) {
					while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_ring
							|| (application_state_handler->on_ring
								&& application_state_handler->on_ring(session) == SWITCH_STATUS_SUCCESS
								&& midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_ring ||
							(application_state_handler->on_ring &&
							 application_state_handler->on_ring(session) == SWITCH_STATUS_SUCCESS
							 && midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_ring(session);
					}
				}
				break;
			case CS_EXECUTE:	/* Execute an Operation */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State EXECUTE\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_execute
					|| (driver_state_handler->on_execute
						&& driver_state_handler->on_execute(session) == SWITCH_STATUS_SUCCESS && midstate == switch_channel_get_state(session->channel))) {
					while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_execute
							|| (application_state_handler->on_execute
								&& application_state_handler->on_execute(session) == SWITCH_STATUS_SUCCESS
								&& midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_execute ||
							(application_state_handler->on_execute &&
							 application_state_handler->on_execute(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_execute(session);
					}
				}
				break;
			case CS_LOOPBACK:	/* loop all data back to source */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State LOOPBACK\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_loopback
					|| (driver_state_handler->on_loopback
						&& driver_state_handler->on_loopback(session) == SWITCH_STATUS_SUCCESS
						&& midstate == switch_channel_get_state(session->channel))) {
					while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_loopback
							|| (application_state_handler->on_loopback
								&& application_state_handler->on_loopback(session) == SWITCH_STATUS_SUCCESS
								&& midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_loopback ||
							(application_state_handler->on_loopback &&
							 application_state_handler->on_loopback(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_loopback(session);
					}
				}
				break;
			case CS_TRANSMIT:	/* send/recieve data to/from another channel */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State TRANSMIT\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_transmit
					|| (driver_state_handler->on_transmit
						&& driver_state_handler->on_transmit(session) == SWITCH_STATUS_SUCCESS
						&& midstate == switch_channel_get_state(session->channel))) {

					while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_transmit
							|| (application_state_handler->on_transmit
								&& application_state_handler->on_transmit(session) == SWITCH_STATUS_SUCCESS
								&& midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_transmit ||
							(application_state_handler->on_transmit &&
							 application_state_handler->on_transmit(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_transmit(session);
					}
				}
				break;
			case CS_HOLD:		/* wait in limbo */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State HOLD\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_hold
					|| (driver_state_handler->on_hold && driver_state_handler->on_hold(session) == SWITCH_STATUS_SUCCESS
						&& midstate == switch_channel_get_state(session->channel))) {

					while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hold
							|| (application_state_handler->on_hold
								&& application_state_handler->on_hold(session) == SWITCH_STATUS_SUCCESS
								&& midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hold ||
							(application_state_handler->on_hold &&
							 application_state_handler->on_hold(session) == SWITCH_STATUS_SUCCESS
							 && midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_hold(session);
					}
				}
				break;
			case CS_HIBERNATE:	/* wait in limbo */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State HIBERNATE\n", switch_channel_get_name(session->channel));
				if (!driver_state_handler->on_hibernate
					|| (driver_state_handler->on_hibernate
						&& driver_state_handler->on_hibernate(session) == SWITCH_STATUS_SUCCESS
						&& midstate == switch_channel_get_state(session->channel))) {

					while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hibernate
							|| (application_state_handler->on_hibernate
								&& application_state_handler->on_hibernate(session) == SWITCH_STATUS_SUCCESS
								&& midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					index = 0;
					while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) {
						if (!application_state_handler || !application_state_handler->on_hibernate ||
							(application_state_handler->on_hibernate &&
							 application_state_handler->on_hibernate(session) == SWITCH_STATUS_SUCCESS &&
							 midstate == switch_channel_get_state(session->channel))) {
							proceed++;
							continue;
						} else {
							proceed = 0;
							break;
						}
					}
					if (proceed) {
						switch_core_standard_on_hibernate(session);
					}
				}
				break;
			}

			if (midstate == CS_DONE) {
				break;
			}

			laststate = midstate;
		}


		endstate = switch_channel_get_state(session->channel);


		if (midstate == endstate) {
			switch_thread_cond_wait(session->cond, session->mutex);
		}

	}
  done:
	switch_mutex_unlock(session->mutex);

	if (runtime.crash_prot) {
		apr_hash_set(stack_table, &thread_id, sizeof(thread_id), NULL);
	}
	session->thread_running = 0;

}
