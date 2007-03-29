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
 * switch_ivr_bridge.c -- IVR Library 
 *
 */
#include <switch.h>

static const switch_state_handler_table_t audio_bridge_peer_state_handlers;
static const switch_state_handler_table_t originate_state_handlers;

/* Bridge Related Stuff*/
/*********************************************************************************/
struct audio_bridge_data {
	switch_core_session_t *session_a;
	switch_core_session_t *session_b;
	int running;
};

static void *audio_bridge_thread(switch_thread_t *thread, void *obj)
{
	switch_core_thread_session_t *his_thread, *data = obj;
	int *stream_id_p;
	int stream_id = 0, pre_b = 0, ans_a = 0, ans_b = 0, originator = 0;
	switch_input_callback_function_t input_callback;
	switch_core_session_message_t *message, msg = { 0 };
	void *user_data;

	switch_channel_t *chan_a, *chan_b;
	switch_frame_t *read_frame;
	switch_core_session_t *session_a, *session_b;

	assert(!thread || thread);

	session_a = data->objs[0];
	session_b = data->objs[1];

	stream_id_p = data->objs[2];
	input_callback = data->input_callback;
	user_data = data->objs[4];
	his_thread = data->objs[5];

	if (stream_id_p) {
		stream_id = *stream_id_p;
	}

	chan_a = switch_core_session_get_channel(session_a);
	chan_b = switch_core_session_get_channel(session_b);

	ans_a = switch_channel_test_flag(chan_a, CF_ANSWERED);
	if ((originator = switch_channel_test_flag(chan_a, CF_ORIGINATOR))) {
		pre_b = switch_channel_test_flag(chan_a, CF_EARLY_MEDIA);
		ans_b = switch_channel_test_flag(chan_b, CF_ANSWERED);
	}
	switch_core_session_read_lock(session_a);
	switch_core_session_read_lock(session_b);

	switch_channel_set_flag(chan_a, CF_BRIDGED);

	while (switch_channel_ready(chan_a) && data->running > 0 && his_thread->running > 0) {
		switch_channel_state_t b_state = switch_channel_get_state(chan_b);
		switch_status_t status;
		switch_event_t *event;

		switch (b_state) {
		case CS_HANGUP:
		case CS_DONE:
			switch_mutex_lock(data->mutex);
			data->running = -1;
			switch_mutex_unlock(data->mutex);
			continue;
		default:
			break;
		}

		if (switch_channel_test_flag(chan_a, CF_TRANSFER)) {
			switch_channel_clear_flag(chan_a, CF_HOLD);
			switch_channel_clear_flag(chan_a, CF_SUSPEND);
			break;
		}

		if (switch_core_session_dequeue_private_event(session_a, &event) == SWITCH_STATUS_SUCCESS) {
			switch_channel_set_flag(chan_b, CF_SUSPEND);
			switch_ivr_parse_event(session_a, event);
			switch_channel_clear_flag(chan_b, CF_SUSPEND);
			switch_event_destroy(&event);
		}

		/* if 1 channel has DTMF pass it to the other */
		if (switch_channel_has_dtmf(chan_a)) {
			char dtmf[128];
			switch_channel_dequeue_dtmf(chan_a, dtmf, sizeof(dtmf));
			switch_core_session_send_dtmf(session_b, dtmf);

			if (input_callback) {
				if (input_callback(session_a, dtmf, SWITCH_INPUT_TYPE_DTMF, user_data, 0) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s ended call via DTMF\n",
									  switch_channel_get_name(chan_a));
					switch_mutex_lock(data->mutex);
					data->running = -1;
					switch_mutex_unlock(data->mutex);
					break;
				}
			}
		}

		if (switch_core_session_dequeue_event(session_a, &event) == SWITCH_STATUS_SUCCESS) {
			if (input_callback) {
				status = input_callback(session_a, event, SWITCH_INPUT_TYPE_EVENT, user_data, 0);
			}

			if (event->event_id != SWITCH_EVENT_MESSAGE
				|| switch_core_session_receive_event(session_b, &event) != SWITCH_STATUS_SUCCESS) {
				switch_event_destroy(&event);
			}

		}

		if (switch_core_session_dequeue_message(session_b, &message) == SWITCH_STATUS_SUCCESS) {
			switch_core_session_receive_message(session_a, message);
			if (switch_test_flag(message, SCSMF_DYNAMIC)) {
				switch_safe_free(message);
			} else {
				message = NULL;
			}
		}

		if (!ans_a && originator) {

			if (!ans_b && switch_channel_test_flag(chan_b, CF_ANSWERED)) {
				switch_channel_answer(chan_a);
				ans_a++;
			} else if (!pre_b && switch_channel_test_flag(chan_b, CF_EARLY_MEDIA)) {
				switch_channel_pre_answer(chan_a);
				pre_b++;
			}
			if (!pre_b) {
				switch_yield(10000);
				continue;
			}
		}


		if (switch_channel_test_flag(chan_a, CF_SUSPEND) || switch_channel_test_flag(chan_b, CF_SUSPEND)) {
			switch_yield(10000);
			continue;
		}

		/* read audio from 1 channel and write it to the other */
		status = switch_core_session_read_frame(session_a, &read_frame, -1, stream_id);

		if (SWITCH_READ_ACCEPTABLE(status)) {
			if (status != SWITCH_STATUS_BREAK && !switch_channel_test_flag(chan_a, CF_HOLD)) {
				if (switch_core_session_write_frame(session_b, read_frame, -1, stream_id) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "write: %s Bad Frame....[%u] Bubye!\n",
									  switch_channel_get_name(chan_b), read_frame->datalen);
					switch_mutex_lock(data->mutex);
					data->running = -1;
					switch_mutex_unlock(data->mutex);
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "read: %s Bad Frame.... Bubye!\n",
							  switch_channel_get_name(chan_a));
			switch_mutex_lock(data->mutex);
			data->running = -1;
			switch_mutex_unlock(data->mutex);
		}
	}

	switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);

	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	msg.from = __FILE__;
	switch_core_session_receive_message(session_a, &msg);

	switch_channel_set_variable(chan_a, SWITCH_BRIDGE_VARIABLE, NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BRIDGE THREAD DONE [%s]\n",
					  switch_channel_get_name(chan_a));

	switch_channel_clear_flag(chan_a, CF_BRIDGED);
	switch_mutex_lock(data->mutex);
	data->running = 0;
	switch_mutex_unlock(data->mutex);

	switch_core_session_rwunlock(session_a);
	switch_core_session_rwunlock(session_b);
	return NULL;
}

static switch_status_t audio_bridge_on_loopback(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	void *arg;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if ((arg = switch_channel_get_private(channel, "_bridge_"))) {
		switch_channel_set_private(channel, "_bridge_", NULL);
		audio_bridge_thread(NULL, (void *) arg);
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}
	switch_channel_clear_state_handler(channel, &audio_bridge_peer_state_handlers);

	if (!switch_channel_test_flag(channel, CF_TRANSFER)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	}

	return SWITCH_STATUS_FALSE;
}


static switch_status_t audio_bridge_on_ring(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CUSTOM RING\n");

	/* put the channel in a passive state so we can loop audio to it */
	switch_channel_set_state(channel, CS_HOLD);
	return SWITCH_STATUS_FALSE;
}

static switch_status_t audio_bridge_on_hold(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CUSTOM HOLD\n");

	/* put the channel in a passive state so we can loop audio to it */
	return SWITCH_STATUS_FALSE;
}

static const switch_state_handler_table_t audio_bridge_peer_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ audio_bridge_on_ring,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_loopback */ audio_bridge_on_loopback,
	/*.on_transmit */ NULL,
	/*.on_hold */ audio_bridge_on_hold,
};



static switch_status_t uuid_bridge_on_transmit(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	switch_core_session_t *other_session;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "CUSTOM TRANSMIT\n");
	switch_channel_clear_state_handler(channel, NULL);


	if (!switch_channel_test_flag(channel, CF_ORIGINATOR)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((other_session = switch_channel_get_private(channel, SWITCH_UUID_BRIDGE))) {
		switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
		switch_channel_state_t state = switch_channel_get_state(other_channel);
		switch_channel_state_t mystate = switch_channel_get_state(channel);
		switch_event_t *event;
		uint8_t ready_a, ready_b;
		switch_caller_profile_t *profile, *new_profile;


		switch_channel_clear_flag(channel, CF_TRANSFER);
		switch_channel_set_private(channel, SWITCH_UUID_BRIDGE, NULL);

		while (mystate <= CS_HANGUP && state <= CS_HANGUP && !switch_channel_test_flag(other_channel, CF_TAGGED)) {
			switch_yield(1000);
			state = switch_channel_get_state(other_channel);
			mystate = switch_channel_get_state(channel);
		}

		switch_channel_clear_flag(other_channel, CF_TRANSFER);
		switch_channel_clear_flag(other_channel, CF_TAGGED);


		switch_core_session_reset(session);
		switch_core_session_reset(other_session);

		ready_a = switch_channel_ready(channel);
		ready_b = switch_channel_ready(other_channel);

		if (!ready_a || !ready_b) {
			if (!ready_a) {
				switch_channel_hangup(other_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}

			if (!ready_b) {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
			return SWITCH_STATUS_FALSE;
		}

		/* add another profile to both sessions for CDR's sake */
		if ((profile = switch_channel_get_caller_profile(channel))) {
			new_profile = switch_caller_profile_clone(session, profile);
			new_profile->destination_number =
				switch_core_session_strdup(session, switch_core_session_get_uuid(other_session));
			switch_channel_set_caller_profile(channel, new_profile);
		}

		if ((profile = switch_channel_get_caller_profile(other_channel))) {
			new_profile = switch_caller_profile_clone(other_session, profile);
			new_profile->destination_number =
				switch_core_session_strdup(other_session, switch_core_session_get_uuid(session));
			switch_channel_set_caller_profile(other_channel, new_profile);
		}

		/* fire events that will change the data table from "show channels" */
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "uuid_bridge");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s",
									switch_core_session_get_uuid(other_session));
			switch_event_fire(&event);
		}

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(other_channel, event);
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "uuid_bridge");
			switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s",
									switch_core_session_get_uuid(session));
			switch_event_fire(&event);
		}

		switch_ivr_multi_threaded_bridge(session, other_session, NULL, NULL, NULL);
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}



	return SWITCH_STATUS_FALSE;
}

static const switch_state_handler_table_t uuid_bridge_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_loopback */ NULL,
	/*.on_transmit */ uuid_bridge_on_transmit,
	/*.on_hold */ NULL
};

static switch_status_t signal_bridge_on_hibernate(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);
	switch_channel_clear_flag(channel, CF_TRANSFER);

	switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE,
								switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t signal_bridge_on_hangup(switch_core_session_t *session)
{
	char *uuid;
	switch_channel_t *channel = NULL;
	switch_core_session_t *other_session;
	switch_event_t *event;

	channel = switch_core_session_get_channel(session);
	assert(channel != NULL);

	if (switch_channel_test_flag(channel, CF_ORIGINATOR)) {
		switch_channel_clear_flag(channel, CF_ORIGINATOR);
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
	}


	if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE))
		&& (other_session = switch_core_session_locate(uuid))) {
		switch_channel_t *other_channel = NULL;

		other_channel = switch_core_session_get_channel(other_session);
		assert(other_channel != NULL);

		switch_channel_set_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);
		switch_channel_set_variable(other_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);

		switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, NULL);
		switch_channel_set_variable(other_channel, SWITCH_BRIDGE_VARIABLE, NULL);

		if (switch_channel_get_state(other_channel) < CS_HANGUP &&
			switch_true(switch_channel_get_variable(other_channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE))) {
			switch_channel_hangup(other_channel, switch_channel_get_cause(channel));
		} else {
			switch_channel_set_state(other_channel, CS_EXECUTE);
		}
		switch_core_session_rwunlock(other_session);
	}


	return SWITCH_STATUS_SUCCESS;
}

static const switch_state_handler_table_t signal_bridge_state_handlers = {
	/*.on_init */ NULL,
	/*.on_ring */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ signal_bridge_on_hangup,
	/*.on_loopback */ NULL,
	/*.on_transmit */ NULL,
	/*.on_hold */ NULL,
	/*.on_hibernate */ signal_bridge_on_hibernate
};

SWITCH_DECLARE(switch_status_t) switch_ivr_signal_bridge(switch_core_session_t *session,
														 switch_core_session_t *peer_session)
{
	switch_channel_t *caller_channel, *peer_channel;
	switch_event_t *event;

	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	peer_channel = switch_core_session_get_channel(peer_session);
	assert(peer_channel != NULL);

	if (!switch_channel_ready(peer_channel)) {
		switch_channel_hangup(caller_channel, switch_channel_get_cause(peer_channel));
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_channel_ready(caller_channel)) {
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_ORIGINATOR_CANCEL);
		return SWITCH_STATUS_FALSE;
	}

	switch_channel_set_variable(caller_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE,
								switch_core_session_get_uuid(peer_session));
	switch_channel_set_variable(peer_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));

	switch_channel_set_flag(caller_channel, CF_ORIGINATOR);

	switch_channel_clear_state_handler(caller_channel, NULL);
	switch_channel_clear_state_handler(peer_channel, NULL);

	switch_channel_add_state_handler(caller_channel, &signal_bridge_state_handlers);
	switch_channel_add_state_handler(peer_channel, &signal_bridge_state_handlers);


	/* fire events that will change the data table from "show channels" */
	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(caller_channel, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "signal_bridge");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s",
								switch_core_session_get_uuid(peer_session));
		switch_event_fire(&event);
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(peer_channel, event);
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application", "signal_bridge");
		switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Application-Data", "%s",
								switch_core_session_get_uuid(session));
		switch_event_fire(&event);
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(caller_channel, event);
		switch_event_fire(&event);
	}

	switch_channel_set_state_flag(caller_channel, CF_TRANSFER);
	switch_channel_set_state_flag(peer_channel, CF_TRANSFER);


	switch_channel_set_state(caller_channel, CS_HIBERNATE);
	switch_channel_set_state(peer_channel, CS_HIBERNATE);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_multi_threaded_bridge(switch_core_session_t *session,
																 switch_core_session_t *peer_session,
																 switch_input_callback_function_t input_callback,
																 void *session_data, void *peer_session_data)
{
	switch_core_thread_session_t *this_audio_thread, *other_audio_thread;
	switch_channel_t *caller_channel, *peer_channel;
	int stream_id = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	caller_channel = switch_core_session_get_channel(session);
	assert(caller_channel != NULL);

	switch_channel_set_flag(caller_channel, CF_ORIGINATOR);

	peer_channel = switch_core_session_get_channel(peer_session);
	assert(peer_channel != NULL);

	other_audio_thread = switch_core_session_alloc(peer_session, sizeof(switch_core_thread_session_t));
	this_audio_thread = switch_core_session_alloc(peer_session, sizeof(switch_core_thread_session_t));

	other_audio_thread->objs[0] = session;
	other_audio_thread->objs[1] = peer_session;
	other_audio_thread->objs[2] = &stream_id;
	other_audio_thread->input_callback = input_callback;
	other_audio_thread->objs[4] = session_data;
	other_audio_thread->objs[5] = this_audio_thread;
	other_audio_thread->running = 5;
	switch_mutex_init(&other_audio_thread->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

	this_audio_thread->objs[0] = peer_session;
	this_audio_thread->objs[1] = session;
	this_audio_thread->objs[2] = &stream_id;
	this_audio_thread->input_callback = input_callback;
	this_audio_thread->objs[4] = peer_session_data;
	this_audio_thread->objs[5] = other_audio_thread;
	this_audio_thread->running = 2;
	switch_mutex_init(&this_audio_thread->mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(peer_session));

	switch_channel_add_state_handler(peer_channel, &audio_bridge_peer_state_handlers);

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) && !switch_channel_test_flag(caller_channel, CF_ANSWERED)) {
		switch_channel_answer(caller_channel);
	}

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {
		switch_event_t *event;
		switch_core_session_message_t msg = { 0 };

		switch_channel_set_state(peer_channel, CS_HOLD);

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(caller_channel, event);
			switch_event_fire(&event);
		}

		if (switch_core_session_read_lock(peer_session) == SWITCH_STATUS_SUCCESS) {
			switch_channel_set_variable(caller_channel, SWITCH_BRIDGE_VARIABLE,
										switch_core_session_get_uuid(peer_session));
			switch_channel_set_variable(peer_channel, SWITCH_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));

			msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
			msg.from = __FILE__;
			msg.pointer_arg = session;

			switch_core_session_receive_message(peer_session, &msg);

			if (!msg.pointer_arg) {
				status = SWITCH_STATUS_FALSE;
				switch_core_session_rwunlock(peer_session);
				goto done;
			}

			msg.pointer_arg = peer_session;
			switch_core_session_receive_message(session, &msg);

			if (!msg.pointer_arg) {
				status = SWITCH_STATUS_FALSE;
				switch_core_session_rwunlock(peer_session);
				goto done;
			}


			switch_channel_set_private(peer_channel, "_bridge_", other_audio_thread);
			switch_channel_set_state(peer_channel, CS_LOOPBACK);
			audio_bridge_thread(NULL, (void *) this_audio_thread);

			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
				switch_channel_event_set_data(caller_channel, event);
				switch_event_fire(&event);
			}

			this_audio_thread->objs[0] = NULL;
			this_audio_thread->objs[1] = NULL;
			this_audio_thread->objs[2] = NULL;
			this_audio_thread->input_callback = NULL;
			this_audio_thread->objs[4] = NULL;
			this_audio_thread->objs[5] = NULL;
			switch_mutex_lock(this_audio_thread->mutex);
			this_audio_thread->running = 0;
			switch_mutex_unlock(this_audio_thread->mutex);

			switch_channel_clear_flag(caller_channel, CF_ORIGINATOR);

			if (other_audio_thread->running > 0) {
				switch_mutex_lock(other_audio_thread->mutex);
				other_audio_thread->running = -1;
				switch_mutex_unlock(other_audio_thread->mutex);
				while (other_audio_thread->running) {
					switch_yield(1000);
				}
			}

			switch_core_session_rwunlock(peer_session);

		} else {
			status = SWITCH_STATUS_FALSE;
		}
	} else {
		status = SWITCH_STATUS_FALSE;
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Bridge Failed %s->%s\n",
						  switch_channel_get_name(caller_channel), switch_channel_get_name(peer_channel)
			);
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ANSWER);
	}

  done:

	if (switch_channel_get_state(caller_channel) < CS_HANGUP &&
		switch_true(switch_channel_get_variable(caller_channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE))) {
		switch_channel_hangup(caller_channel, switch_channel_get_cause(peer_channel));
	}

	return status;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_uuid_bridge(char *originator_uuid, char *originatee_uuid)
{
	switch_core_session_t *originator_session, *originatee_session;
	switch_channel_t *originator_channel, *originatee_channel;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if ((originator_session = switch_core_session_locate(originator_uuid))) {
		if ((originatee_session = switch_core_session_locate(originatee_uuid))) {
			originator_channel = switch_core_session_get_channel(originator_session);
			originatee_channel = switch_core_session_get_channel(originatee_session);

			/* override transmit state for originator_channel to bridge to originatee_channel 
			 * install pointer to originatee_session into originator_channel
			 * set CF_TRANSFER on both channels and change state to CS_TRANSMIT to
			 * inturrupt anything they are already doing.
			 * originatee_session will fall asleep and originator_session will bridge to it
			 */

			switch_channel_clear_state_handler(originator_channel, NULL);
			switch_channel_clear_state_handler(originatee_channel, NULL);
			switch_channel_set_flag(originator_channel, CF_ORIGINATOR);
			switch_channel_add_state_handler(originator_channel, &uuid_bridge_state_handlers);
			switch_channel_add_state_handler(originatee_channel, &uuid_bridge_state_handlers);
			switch_channel_set_flag(originatee_channel, CF_TAGGED);
			switch_channel_set_private(originator_channel, SWITCH_UUID_BRIDGE, originatee_session);

			/* switch_channel_set_state_flag sets flags you want to be set when the next state change happens */
			switch_channel_set_state_flag(originator_channel, CF_TRANSFER);
			switch_channel_set_state_flag(originatee_channel, CF_TRANSFER);

			/* release the read locks we have on the channels */
			switch_core_session_rwunlock(originator_session);
			switch_core_session_rwunlock(originatee_session);

			/* change the states and let the chips fall where they may */
			switch_channel_set_state(originator_channel, CS_TRANSMIT);
			switch_channel_set_state(originatee_channel, CS_TRANSMIT);

			status = SWITCH_STATUS_SUCCESS;

			while (switch_channel_get_state(originatee_channel) < CS_HANGUP
				   && switch_channel_test_flag(originatee_channel, CF_TAGGED)) {
				switch_yield(20000);
			}
		} else {
			switch_core_session_rwunlock(originator_session);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "no channel for originatee uuid %s\n",
							  originatee_uuid);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "no channel for originator uuid %s\n",
						  originator_uuid);
	}

	return status;

}
