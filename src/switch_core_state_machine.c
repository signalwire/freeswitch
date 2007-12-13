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

static void switch_core_standard_on_reset(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard RESET %s\n",
					  switch_channel_get_name(session->channel));
	
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

 top:
	switch_channel_clear_flag(session->channel, CF_RESET);

	if ((extension = switch_channel_get_caller_extension(session->channel)) == 0) {
		switch_channel_hangup(session->channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return;
	}

	while (switch_channel_get_state(session->channel) == CS_EXECUTE && extension->current_application) {
		char *expanded = NULL;
		int nomedia = 0;
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Execute %s(%s)\n",
						  extension->current_application->application_name, switch_str_nil(extension->current_application->application_data));
		if ((application_interface = switch_loadable_module_get_application_interface(extension->current_application->application_name)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Application %s\n", extension->current_application->application_name);
			switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}
		
		if (!application_interface->application_function) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Function for %s\n", extension->current_application->application_name);
			switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			return;
		}
		
		if (switch_channel_test_flag(session->channel, CF_BYPASS_MEDIA) && !switch_test_flag(application_interface, SAF_SUPPORT_NOMEDIA)) {
			switch_ivr_media(session->uuid_str, SMF_NONE);
			nomedia++;
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Application %s Requires media!\n",
							  extension->current_application->application_name);
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

		if (switch_channel_test_flag(session->channel, CF_RESET)) {
			goto top;
		}

		if (nomedia) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Application %s Releasing media\n",
							  extension->current_application->application_name);
			switch_ivr_nomedia(session->uuid_str, SMF_NONE);
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
	switch_assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard TRANSMIT\n");
}

static void switch_core_standard_on_park(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard PARK\n");
	switch_channel_clear_flag(session->channel, CF_TRANSFER);
	switch_core_session_reset(session);
	switch_ivr_park(session, NULL);
}

static void switch_core_standard_on_hold(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard HOLD\n");
}

static void switch_core_standard_on_hibernate(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Standard HIBERNATE\n");
}

#include <sqlite3.h>
#include "../../../libs/sqlite/src/hash.h"

//static switch_hash_t *stack_table = NULL;
static Hash stack_table;

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
		&& (env = (jmp_buf *) sqlite3HashFind(&stack_table, &thread_id, sizeof(thread_id)))) {
		//&& (env = (jmp_buf *) switch_core_hash_find(stack_table, (char *)&thread_id, sizeof(thread_id)))) {
		print_trace();
		longjmp(*env, sig);
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Caught signal %d for unmapped thread!", sig);
		abort();
	}
}


void switch_core_state_machine_init(switch_memory_pool_t *pool)
{
	
	if (switch_test_flag((&runtime), SCF_CRASH_PROT)) {
		sqlite3HashInit(&stack_table, SQLITE_HASH_BINARY, 0);
	}
}



#define STATE_MACRO(__STATE, __STATE_STR)						do {	\
		midstate = state;												\
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State %s\n", switch_channel_get_name(session->channel), __STATE_STR);	\
		if (!driver_state_handler->on_##__STATE || (driver_state_handler->on_##__STATE(session) == SWITCH_STATUS_SUCCESS \
													&& midstate == switch_channel_get_state(session->channel))) { \
			while ((application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) { \
				if (!application_state_handler || !application_state_handler->on_##__STATE \
					|| (application_state_handler->on_##__STATE			\
						&& application_state_handler->on_##__STATE(session) == SWITCH_STATUS_SUCCESS \
						&& midstate == switch_channel_get_state(session->channel))) { \
					proceed++;											\
					continue;											\
				} else {												\
					proceed = 0;										\
					break;												\
				}														\
			}															\
			index = 0;													\
			while (proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) { \
				if (!application_state_handler || !application_state_handler->on_##__STATE || \
					(application_state_handler->on_##__STATE &&			\
					 application_state_handler->on_##__STATE(session) == SWITCH_STATUS_SUCCESS \
					 && midstate == switch_channel_get_state(session->channel))) { \
					proceed++;											\
					continue;											\
				} else {												\
					proceed = 0;										\
					break;												\
				}														\
			}															\
			if (proceed) {												\
				switch_core_standard_on_##__STATE(session);				\
			}															\
		}																\
	} while (silly)

SWITCH_DECLARE(void) switch_core_session_run(switch_core_session_t *session)
{
	switch_channel_state_t state = CS_NEW, laststate = CS_HANGUP, midstate = CS_DONE, endstate;
	const switch_endpoint_interface_t *endpoint_interface;
	const switch_state_handler_table_t *driver_state_handler = NULL;
	const switch_state_handler_table_t *application_state_handler = NULL;
	switch_thread_id_t thread_id;
	jmp_buf env;
	int sig, silly = 0;

	if (switch_test_flag((&runtime), SCF_CRASH_PROT)) {
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
			sqlite3HashInsert(&stack_table, &thread_id, sizeof(thread_id), (void *)&env);
			//apr_hash_set(stack_table, &thread_id, sizeof(thread_id), &env);
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
	switch_assert(session != NULL);

	session->thread_running = 1;
	endpoint_interface = session->endpoint_interface;
	switch_assert(endpoint_interface != NULL);

	driver_state_handler = endpoint_interface->state_handler;
	switch_assert(driver_state_handler != NULL);

	switch_mutex_lock(session->mutex);

	while ((state = switch_channel_get_state(session->channel)) != CS_DONE) {
		uint8_t exception = 0;
		midstate = state;
		if (switch_channel_test_flag(session->channel, CF_REPEAT_STATE)) {
			switch_channel_clear_flag(session->channel, CF_REPEAT_STATE);
			exception = 1;
		}
		if (state != laststate || state == CS_HANGUP || exception) {
			int index = 0;
			int proceed = 1;
			
			switch_channel_set_running_state(session->channel);

			switch (state) {
			case CS_NEW:		/* Just created, Waiting for first instructions */
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "(%s) State NEW\n", switch_channel_get_name(session->channel));
				break;
			case CS_DONE:
				goto done;
			case CS_HANGUP:	    /* Deactivate and end the thread */
				STATE_MACRO(hangup, "HANGUP");
				goto done;
			case CS_INIT:		/* Basic setup tasks */
				STATE_MACRO(init, "INIT");
				break;
			case CS_RING:		/* Look for a dialplan and find something to do */
				STATE_MACRO(ring, "RING");
				break;
			case CS_RESET:		/* Reset */
				STATE_MACRO(reset, "RESET");
				break;
			case CS_EXECUTE:	/* Execute an Operation */
				STATE_MACRO(execute, "EXECUTE");
				break;
			case CS_LOOPBACK:	/* loop all data back to source */
				STATE_MACRO(loopback, "LOOPBACK");
				break;
			case CS_TRANSMIT:	/* send/recieve data to/from another channel */
				STATE_MACRO(transmit, "TRANSMIT");
				break;
			case CS_PARK:		/* wait in limbo */
				STATE_MACRO(park, "PARK");
				break;
			case CS_HOLD:		/* wait in limbo */
				STATE_MACRO(hold, "HOLD");
				break;
			case CS_HIBERNATE:	/* sleep */
				STATE_MACRO(hibernate, "HIBERNATE");
				break;
			}
			
			if (midstate == CS_DONE) {
				break;
			}

			laststate = midstate;
		}


		endstate = switch_channel_get_state(session->channel);


		if (midstate == endstate) {
			if (endstate == CS_NEW) {
				switch_yield(1000);
			} else {
				switch_thread_cond_wait(session->cond, session->mutex);
			}
		}

	}
  done:
	switch_mutex_unlock(session->mutex);

	if (switch_test_flag((&runtime), SCF_CRASH_PROT)) {
		sqlite3HashInsert(&stack_table, &thread_id, sizeof(thread_id), NULL);
		//apr_hash_set(stack_table, &thread_id, sizeof(thread_id), NULL);
	}
	session->thread_running = 0;

}
