/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2012, Anthony Minessale II <anthm@freeswitch.org>
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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Michael Jerris <mike@jerris.com>
 * Paul D. Tinsley <pdt at jackhammer.org>
 *
 *
 * switch_core_state_machine.c -- Main Core Library (state machine)
 *
 */

#include <switch.h>
#include "private/switch_core_pvt.h"

static void switch_core_standard_on_init(switch_core_session_t *session)
{
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard INIT\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_hangup(switch_core_session_t *session)
{
	switch_caller_extension_t *extension;
	int rec;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard HANGUP, cause: %s\n",
					  switch_channel_get_name(session->channel), switch_channel_cause2str(switch_channel_get_cause(session->channel)));


	rec = switch_channel_test_flag(session->channel, CF_RECOVERING);
	switch_channel_clear_flag(session->channel, CF_RECOVERING);

	if (!rec) {
		switch_core_recovery_untrack(session, SWITCH_TRUE);
	}

	
	if (!switch_channel_test_flag(session->channel, CF_ZOMBIE_EXEC)) {
		return;
	}

	if ((extension = switch_channel_get_caller_extension(session->channel)) == 0) {
		return;
	}

	while(extension->current_application) {
		switch_caller_application_t *current_application = extension->current_application;
		switch_status_t status;

		extension->current_application = extension->current_application->next;

		status = switch_core_session_execute_application(session,
														 current_application->application_name, current_application->application_data);
														 
		
		if (status != SWITCH_STATUS_SUCCESS && status != SWITCH_STATUS_IGNORE) {
			return;
		}
	}





}

static void switch_core_standard_on_reporting(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard REPORTING, cause: %s\n",
					  switch_channel_get_name(session->channel), switch_channel_cause2str(switch_channel_get_cause(session->channel)));
}

static void switch_core_standard_on_destroy(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard DESTROY\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_reset(switch_core_session_t *session)
{
	switch_channel_set_variable(session->channel, "call_uuid", switch_core_session_get_uuid(session));

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard RESET\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_routing(switch_core_session_t *session)
{
	switch_dialplan_interface_t *dialplan_interface = NULL;
	switch_caller_profile_t *caller_profile;
	switch_caller_extension_t *extension = NULL;
	char *expanded = NULL;
	char *dpstr = NULL;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard ROUTING\n", switch_channel_get_name(session->channel));

	switch_channel_set_variable(session->channel, "call_uuid", switch_core_session_get_uuid(session));
		
	if ((switch_channel_test_flag(session->channel, CF_ANSWERED) ||
		 switch_channel_test_flag(session->channel, CF_EARLY_MEDIA) ||
		 switch_channel_test_flag(session->channel, CF_SIGNAL_BRIDGE_TTL)) && switch_channel_test_flag(session->channel, CF_PROXY_MODE)) {
		switch_ivr_media(session->uuid_str, SMF_NONE);
	}

	if ((caller_profile = switch_channel_get_caller_profile(session->channel)) == 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't get profile!\n");
		switch_channel_hangup(session->channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		return;
	} else {
		char *dp[25];
		int argc, x, count = 0;

		if ((extension = switch_channel_get_queued_extension(session->channel))) {
			switch_channel_set_caller_extension(session->channel, extension);
			switch_channel_set_state(session->channel, CS_EXECUTE);
			goto end;
		}

		if (!zstr(caller_profile->dialplan)) {
			if ((dpstr = switch_core_session_strdup(session, caller_profile->dialplan))) {
				expanded = switch_channel_expand_variables(session->channel, dpstr);
				argc = switch_separate_string(expanded, ',', dp, (sizeof(dp) / sizeof(dp[0])));
				for (x = 0; x < argc; x++) {
					char *dpname = dp[x];
					char *dparg = NULL;

					if (dpname) {
						if ((dparg = strchr(dpname, ':'))) {
							*dparg++ = '\0';
						}
					} else {
						continue;
					}
					if (!(dialplan_interface = switch_loadable_module_get_dialplan_interface(dpname))) {
						continue;
					}

					count++;

					extension = dialplan_interface->hunt_function(session, dparg, NULL);
					UNPROTECT_INTERFACE(dialplan_interface);

					if (extension) {
						switch_channel_set_caller_extension(session->channel, extension);
						switch_channel_set_state(session->channel, CS_EXECUTE);
						goto end;
					}
				}
			}
		}

		if (!count) {
			if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
				if (switch_channel_test_flag(session->channel, CF_ANSWERED)) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
									  "No Dialplan on answered channel, changing state to HANGUP\n");
					switch_channel_hangup(session->channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No Dialplan, changing state to CONSUME_MEDIA\n");
					switch_channel_set_state(session->channel, CS_CONSUME_MEDIA);
				}
				goto end;
			}
		}
	}

	if (!extension) {

		if (switch_ivr_blind_transfer_ack(session, SWITCH_FALSE) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "No Route, Aborting\n");
			switch_channel_hangup(session->channel, SWITCH_CAUSE_NO_ROUTE_DESTINATION);
		}
	}

  end:

	if (expanded && dpstr && expanded != dpstr) {
		free(expanded);
	}
}

static void switch_core_standard_on_execute(switch_core_session_t *session)
{
	switch_caller_extension_t *extension;
	const char *uuid;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard EXECUTE\n", switch_channel_get_name(session->channel));

	switch_channel_set_variable(session->channel, "call_uuid", switch_core_session_get_uuid(session));

	if (switch_channel_get_variable(session->channel, "recovered") && !switch_channel_test_flag(session->channel, CF_RECOVERED)) {
		switch_channel_set_flag(session->channel, CF_RECOVERED);
	}

  top:
	switch_channel_clear_flag(session->channel, CF_RESET);
	
	if ((extension = switch_channel_get_caller_extension(session->channel)) == 0) {
		switch_channel_hangup(session->channel, SWITCH_CAUSE_NORMAL_CLEARING);
		return;
	}

	while (switch_channel_get_state(session->channel) == CS_EXECUTE && extension->current_application) {
		switch_caller_application_t *current_application = extension->current_application;

		extension->current_application = extension->current_application->next;

		if (switch_core_session_execute_application(session,
													current_application->application_name,
													current_application->application_data) != SWITCH_STATUS_SUCCESS) {
			return;
		}

		if (switch_channel_test_flag(session->channel, CF_RESET)) {
			goto top;
		}

	}

	if (switch_channel_ready(session->channel) && switch_channel_get_state(session->channel) == CS_EXECUTE && 
		switch_channel_test_flag(session->channel, CF_CONFIRM_BLIND_TRANSFER) && 
		(uuid = switch_channel_get_variable(session->channel, "blind_transfer_uuid"))) {
		switch_core_session_t *other_session;

		if ((other_session = switch_core_session_locate(uuid))) {
			switch_core_session_message_t msg = { 0 };			
			msg.message_id = SWITCH_MESSAGE_INDICATE_BLIND_TRANSFER_RESPONSE;
			msg.from = __FILE__;
			msg.numeric_arg = 0;
			switch_core_session_receive_message(other_session, &msg);
			switch_core_session_rwunlock(other_session);

			switch_channel_set_variable(session->channel, "park_timeout", "10:blind_transfer");
			switch_channel_set_state(session->channel, CS_PARK);
			switch_channel_clear_flag(session->channel, CF_CONFIRM_BLIND_TRANSFER);
		}
	}
	
	if (switch_channel_ready(session->channel) && switch_channel_get_state(session->channel) == CS_EXECUTE) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "%s has executed the last dialplan instruction, hanging up.\n",
						  switch_channel_get_name(session->channel));
		switch_channel_hangup(session->channel, SWITCH_CAUSE_NORMAL_CLEARING);
	}
}

static void switch_core_standard_on_exchange_media(switch_core_session_t *session)
{

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard EXCHANGE_MEDIA\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_soft_execute(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard SOFT_EXECUTE\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_park(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard PARK\n", switch_channel_get_name(session->channel));
	switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);
	switch_ivr_park(session, NULL);
}

static void switch_core_standard_on_consume_media(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard CONSUME_MEDIA\n", switch_channel_get_name(session->channel));
}

static void switch_core_standard_on_hibernate(switch_core_session_t *session)
{
	switch_assert(session != NULL);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Standard HIBERNATE\n", switch_channel_get_name(session->channel));
}

void switch_core_state_machine_init(switch_memory_pool_t *pool)
{
	return;
}

#define STATE_MACRO(__STATE, __STATE_STR)						do {	\
		midstate = state;												\
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "(%s) State %s\n", switch_channel_get_name(session->channel), __STATE_STR);	\
		if (!driver_state_handler->on_##__STATE || (driver_state_handler->on_##__STATE(session) == SWITCH_STATUS_SUCCESS \
													)) {				\
			while (do_extra_handlers && (application_state_handler = switch_channel_get_state_handler(session->channel, index++)) != 0) { \
				if (!application_state_handler || !application_state_handler->on_##__STATE \
					|| (application_state_handler->on_##__STATE			\
						&& application_state_handler->on_##__STATE(session) == SWITCH_STATUS_SUCCESS \
						)) {											\
					proceed++;											\
					continue;											\
				} else {												\
					proceed = 0;										\
					break;												\
				}														\
			}															\
			index = 0;													\
			if (!proceed) global_proceed = 0;							\
			proceed = 1;												\
			while (do_extra_handlers && proceed && (application_state_handler = switch_core_get_state_handler(index++)) != 0) { \
				if (!application_state_handler || !application_state_handler->on_##__STATE || \
					(application_state_handler->on_##__STATE &&			\
					 application_state_handler->on_##__STATE(session) == SWITCH_STATUS_SUCCESS \
					 )) {												\
					proceed++;											\
					continue;											\
				} else {												\
					proceed = 0;										\
					break;												\
				}														\
			}															\
			if (!proceed || midstate != switch_channel_get_state(session->channel)) global_proceed = 0; \
			if (global_proceed) {										\
				switch_core_standard_on_##__STATE(session);				\
			}															\
		}																\
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "(%s) State %s going to sleep\n", switch_channel_get_name(session->channel), __STATE_STR); \
	} while (silly)


static void check_presence(switch_core_session_t *session)
{
	switch_channel_state_t state = switch_channel_get_running_state(session->channel);

	if (state == CS_ROUTING || state == CS_HANGUP) {
		if (switch_channel_get_cause(session->channel) == SWITCH_CAUSE_LOSE_RACE) {
			switch_channel_presence(session->channel, "unknown", "cancelled", NULL);
			switch_channel_set_variable(session->channel, "presence_call_info", NULL);
		} else {
			switch_channel_presence(session->channel, "unknown", switch_channel_state_name(state), NULL);
		}
	}
}



SWITCH_DECLARE(void) switch_core_session_run(switch_core_session_t *session)
{
	switch_channel_state_t state = CS_NEW, midstate = CS_DESTROY, endstate;
	const switch_endpoint_interface_t *endpoint_interface;
	const switch_state_handler_table_t *driver_state_handler = NULL;
	const switch_state_handler_table_t *application_state_handler = NULL;
	int silly = 0;
	uint32_t new_loops = 500;

	/*
	   Life of the channel. you have channel and pool in your session
	   everywhere you go you use the session to malloc with
	   switch_core_session_alloc(session, <size>)

	   The endpoint module gets the first crack at implementing the state
	   if it wants to, it can cancel the default behavior by returning SWITCH_STATUS_FALSE

	   Next comes the channel's event handler table that can be set by an application
	   which also can veto the next behavior in line by returning SWITCH_STATUS_FALSE

	   Finally the default state behavior is called.


	 */
	switch_assert(session != NULL);

	switch_set_flag(session, SSF_THREAD_RUNNING);
	endpoint_interface = session->endpoint_interface;
	switch_assert(endpoint_interface != NULL);

	driver_state_handler = endpoint_interface->state_handler;
	switch_assert(driver_state_handler != NULL);

	switch_mutex_lock(session->mutex);

	while ((state = switch_channel_get_state(session->channel)) != CS_DESTROY) {

		if (switch_channel_test_flag(session->channel, CF_BLOCK_STATE)) {
			switch_channel_wait_for_flag(session->channel, CF_BLOCK_STATE, SWITCH_FALSE, 0, NULL);
			if ((state = switch_channel_get_state(session->channel)) == CS_DESTROY) {
				break;
			}
		}

		midstate = state;
		if (state != switch_channel_get_running_state(session->channel) || state >= CS_HANGUP) {
			int index = 0;
			int proceed = 1;
			int global_proceed = 1;
			int do_extra_handlers = 1;
			switch_io_event_hook_state_run_t *ptr;
			switch_status_t rstatus = SWITCH_STATUS_SUCCESS;

			switch_channel_set_running_state(session->channel, state);
			switch_channel_clear_flag(session->channel, CF_TRANSFER);
			switch_channel_clear_flag(session->channel, CF_REDIRECT);
			
			if (session->endpoint_interface->io_routines->state_run) {
				rstatus = session->endpoint_interface->io_routines->state_run(session);
			}
			
			if (rstatus == SWITCH_STATUS_SUCCESS) {
				for (ptr = session->event_hooks.state_run; ptr; ptr = ptr->next) {
					if ((rstatus = ptr->state_run(session)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}
			
			switch (state) {
			case CS_NEW:		/* Just created, Waiting for first instructions */
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "(%s) State NEW\n", switch_channel_get_name(session->channel));
				break;
			case CS_DESTROY:
				goto done;
			case CS_REPORTING:	/* Call Detail */
				{
					switch_core_session_reporting_state(session);
					switch_channel_set_state(session->channel, CS_DESTROY);
				}
				goto done;
			case CS_HANGUP:	/* Deactivate and end the thread */
				{
					switch_core_session_hangup_state(session, SWITCH_TRUE);
					switch_channel_set_state(session->channel, CS_REPORTING);
				}

				break;
			case CS_INIT:		/* Basic setup tasks */
				{
					switch_event_t *event;

					STATE_MACRO(init, "INIT");
					
					if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_CREATE) == SWITCH_STATUS_SUCCESS) {
						switch_channel_event_set_data(session->channel, event);
						switch_event_fire(&event);
					}

					if (switch_channel_direction(session->channel) == SWITCH_CALL_DIRECTION_OUTBOUND) {
						if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_ORIGINATE) == SWITCH_STATUS_SUCCESS) {
							switch_channel_event_set_data(session->channel, event);
							switch_event_fire(&event);
						}
					}
				}
				break;
			case CS_ROUTING:	/* Look for a dialplan and find something to do */
				STATE_MACRO(routing, "ROUTING");
				break;
			case CS_RESET:		/* Reset */
				STATE_MACRO(reset, "RESET");
				break;
				/* These other states are intended for prolonged durations so we do not signal lock for them */
			case CS_EXECUTE:	/* Execute an Operation */
				STATE_MACRO(execute, "EXECUTE");
				break;
			case CS_EXCHANGE_MEDIA:	/* loop all data back to source */
				STATE_MACRO(exchange_media, "EXCHANGE_MEDIA");
				break;
			case CS_SOFT_EXECUTE:	/* send/recieve data to/from another channel */
				STATE_MACRO(soft_execute, "SOFT_EXECUTE");
				break;
			case CS_PARK:		/* wait in limbo */
				STATE_MACRO(park, "PARK");
				break;
			case CS_CONSUME_MEDIA:	/* wait in limbo */
				STATE_MACRO(consume_media, "CONSUME_MEDIA");
				break;
			case CS_HIBERNATE:	/* sleep */
				STATE_MACRO(hibernate, "HIBERNATE");
				break;
			case CS_NONE:
				abort();
				break;
			}

			check_presence(session);

			if (midstate == CS_DESTROY) {
				break;
			}

		}

		endstate = switch_channel_get_state(session->channel);

		if (endstate == switch_channel_get_running_state(session->channel)) {
			if (endstate == CS_NEW) {
				switch_yield(20000);
				switch_ivr_parse_all_events(session);
				if (!--new_loops) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "%s %s Abandoned\n",
									  session->uuid_str, switch_core_session_get_name(session));
					switch_channel_set_flag(session->channel, CF_NO_CDR);
					switch_channel_hangup(session->channel, SWITCH_CAUSE_WRONG_CALL_STATE);
				}
			} else {
				switch_ivr_parse_all_events(session);
				switch_ivr_parse_all_events(session);

				if (switch_channel_get_state(session->channel) == switch_channel_get_running_state(session->channel)) {
					switch_channel_state_thread_lock(session->channel);
					switch_channel_set_flag(session->channel, CF_THREAD_SLEEPING);
					if (switch_channel_get_state(session->channel) == switch_channel_get_running_state(session->channel)) {
						switch_ivr_parse_all_events(session);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s session thread sleep state: %s!\n", 
										  switch_channel_get_name(session->channel),
										  switch_channel_state_name(switch_channel_get_running_state(session->channel)));
						switch_thread_cond_wait(session->cond, session->mutex);
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG1, "%s session thread wake state: %s!\n", 
										  switch_channel_get_name(session->channel),
										  switch_channel_state_name(switch_channel_get_running_state(session->channel)));


					}
					switch_channel_clear_flag(session->channel, CF_THREAD_SLEEPING);
					switch_channel_state_thread_unlock(session->channel);
				}

				switch_ivr_parse_all_events(session);
				switch_ivr_parse_all_events(session);
			}
		}
	}
  done:
	switch_mutex_unlock(session->mutex);

	switch_clear_flag(session, SSF_THREAD_RUNNING);
}

SWITCH_DECLARE(void) switch_core_session_destroy_state(switch_core_session_t *session)
{
	switch_channel_state_t state = CS_DESTROY, midstate = CS_DESTROY;
	const switch_endpoint_interface_t *endpoint_interface;
	const switch_state_handler_table_t *driver_state_handler = NULL;
	const switch_state_handler_table_t *application_state_handler = NULL;
	int proceed = 1;
	int global_proceed = 1;
	int do_extra_handlers = 1;
	int silly = 0;
	int index = 0;

	switch_channel_set_callstate(session->channel, CCS_DOWN);

	switch_assert(session != NULL);
	switch_channel_set_running_state(session->channel, CS_DESTROY);
	switch_channel_clear_flag(session->channel, CF_TRANSFER);
	switch_channel_clear_flag(session->channel, CF_REDIRECT);

	endpoint_interface = session->endpoint_interface;
	switch_assert(endpoint_interface != NULL);

	driver_state_handler = endpoint_interface->state_handler;
	switch_assert(driver_state_handler != NULL);

	STATE_MACRO(destroy, "DESTROY");

	return;
}

static void api_hook(switch_core_session_t *session, const char *hook_var, int use_session)
{
	if (!zstr(hook_var)) {
		switch_stream_handle_t stream = { 0 };
		char *cmd = strdup(hook_var);
		char *arg = NULL;
		char *expanded = NULL;

		if ((arg = strchr(cmd, ':')) && *(arg + 1) == ':') {
			*arg++ = '\0';
			*arg++ = '\0';
		} else {
			if ((arg = strchr(cmd, ' '))) {
				*arg++ = '\0';
			}
		}

		SWITCH_STANDARD_STREAM(stream);

		switch_channel_get_variables(session->channel, &stream.param_event);
		switch_channel_event_set_data(session->channel, stream.param_event);
		expanded = switch_event_expand_headers(stream.param_event, arg);

		switch_api_execute(cmd, expanded, use_session ? session : NULL, &stream);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Hangup Command %s %s(%s):\n%s\n",
						  use_session ? "with Session" : "with no Session", cmd, switch_str_nil(expanded),
						  switch_str_nil((char *) stream.data) );

		if (expanded != arg) {
			switch_safe_free(expanded);
		}

		switch_safe_free(cmd);
		
		switch_safe_free(stream.data);
	}
}



SWITCH_DECLARE(void) switch_core_session_hangup_state(switch_core_session_t *session, switch_bool_t force)
{
	switch_call_cause_t cause = switch_channel_get_cause(session->channel);
	switch_call_cause_t cause_q850 = switch_channel_get_cause_q850(session->channel);
	int proceed = 1;
	int global_proceed = 1;
	int do_extra_handlers = 1;
	int silly = 0;
	int index = 0;
	switch_channel_state_t state = switch_channel_get_state(session->channel), midstate = state;
	const switch_endpoint_interface_t *endpoint_interface;
	const switch_state_handler_table_t *driver_state_handler = NULL;
	const switch_state_handler_table_t *application_state_handler = NULL;
	const char *hook_var;
	int use_session = 0;

	if (!force) {
		if (!switch_channel_test_flag(session->channel, CF_EARLY_HANGUP) && !switch_test_flag((&runtime), SCF_EARLY_HANGUP)) {
			return;
		}

		if (switch_thread_self() != session->thread_id) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "%s thread mismatch skipping state handler.\n",
							  switch_channel_get_name(session->channel));
			return;
		}
	}

	if (switch_test_flag(session, SSF_HANGUP)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "%s handler already called, skipping state handler.\n",
						  switch_channel_get_name(session->channel));
		return;
	}

	endpoint_interface = session->endpoint_interface;
	switch_assert(endpoint_interface != NULL);

	driver_state_handler = endpoint_interface->state_handler;
	switch_assert(driver_state_handler != NULL);

	switch_channel_set_hangup_time(session->channel);

	switch_core_media_bug_remove_all(session);

	switch_channel_stop_broadcast(session->channel);

	switch_channel_set_variable(session->channel, "hangup_cause", switch_channel_cause2str(cause));
	switch_channel_set_variable_printf(session->channel, "hangup_cause_q850", "%d", cause_q850);
	//switch_channel_presence(session->channel, "unknown", switch_channel_cause2str(cause), NULL);

	switch_channel_set_timestamps(session->channel);

	STATE_MACRO(hangup, "HANGUP");

	if ((hook_var = switch_channel_get_variable(session->channel, SWITCH_API_HANGUP_HOOK_VARIABLE))) {

		if (switch_true(switch_channel_get_variable(session->channel, SWITCH_SESSION_IN_HANGUP_HOOK_VARIABLE))) {
			use_session = 1;
		}

		api_hook(session, hook_var, use_session);
	}

	switch_channel_set_callstate(session->channel, CCS_HANGUP);
	switch_set_flag(session, SSF_HANGUP);

}

SWITCH_DECLARE(void) switch_core_session_reporting_state(switch_core_session_t *session)
{
	switch_channel_state_t state = switch_channel_get_state(session->channel), midstate = state;
	const switch_endpoint_interface_t *endpoint_interface;
	const switch_state_handler_table_t *driver_state_handler = NULL;
	const switch_state_handler_table_t *application_state_handler = NULL;
	int proceed = 1;
	int global_proceed = 1;
	int do_extra_handlers = 1;
	int silly = 0;
	int index = 0;
	const char *var = switch_channel_get_variable(session->channel, SWITCH_PROCESS_CDR_VARIABLE);
	const char *skip_var = switch_channel_get_variable(session->channel, SWITCH_SKIP_CDR_CAUSES_VARIABLE);
	const char *hook_var;
	int use_session = 0;
	switch_event_t *event;
	switch_call_cause_t cause = switch_channel_get_cause(session->channel);

	if (switch_channel_test_flag(session->channel, CF_REPORTING)) {
		return;
	}

	switch_channel_set_flag(session->channel, CF_REPORTING);

	switch_assert(session != NULL);

	endpoint_interface = session->endpoint_interface;
	switch_assert(endpoint_interface != NULL);

	driver_state_handler = endpoint_interface->state_handler;
	switch_assert(driver_state_handler != NULL);

	if (!zstr(var)) {
		if (!strcasecmp(var, "a_only")) {
			if (switch_channel_get_originator_caller_profile(session->channel)) {
				do_extra_handlers = 0;
			}
		} else if (!strcasecmp(var, "b_only")) {
			if (switch_channel_get_originatee_caller_profile(session->channel)) {
				do_extra_handlers = 0;
			}
		} else if (!switch_true(var)) {
			do_extra_handlers = 0;
		}
	}


	if (!zstr(skip_var)) {
		int x, ttl = 0;
		char *list[128] = { 0 };
		char *dup = switch_core_session_strdup(session, skip_var);

		ttl = switch_split(dup, '|', list);

		for(x = 0; x < ttl; x++) {
			if (switch_channel_str2cause(list[x]) == cause) {
				do_extra_handlers = 0;
				break;
			}
		}
	}

	if (switch_channel_test_flag(session->channel, CF_NO_CDR)) {
		do_extra_handlers = 0;
	}


	STATE_MACRO(reporting, "REPORTING");

	if ((hook_var = switch_channel_get_variable(session->channel, SWITCH_API_REPORTING_HOOK_VARIABLE))) {

		if (switch_true(switch_channel_get_variable(session->channel, SWITCH_SESSION_IN_HANGUP_HOOK_VARIABLE))) {
			use_session = 1;
		}

		api_hook(session, hook_var, use_session);
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_HANGUP_COMPLETE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Hangup-Cause", switch_channel_cause2str(cause));
		switch_channel_event_set_data(session->channel, event);
		if (switch_true(switch_channel_get_variable(session->channel, "hangup_complete_with_xml"))) {
			switch_xml_t cdr = NULL;
			char *xml_cdr_text;
			
			if (switch_ivr_generate_xml_cdr(session, &cdr) == SWITCH_STATUS_SUCCESS) {
				xml_cdr_text = switch_xml_toxml(cdr, SWITCH_FALSE);
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "CDR-Attached", "xml");
				switch_event_add_body(event, "%s", xml_cdr_text);
				switch_xml_free(cdr);
				switch_safe_free(xml_cdr_text);
			}
		}
		switch_event_fire(&event);
	}



	return;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:t
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
