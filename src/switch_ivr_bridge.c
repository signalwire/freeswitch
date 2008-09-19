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

/* Bridge Related Stuff*/
/*********************************************************************************/

#ifdef SWITCH_VIDEO_IN_THREADS
struct vid_helper {
	switch_core_session_t *session_a;
	switch_core_session_t *session_b;
	int up;
};

static void *SWITCH_THREAD_FUNC video_bridge_thread(switch_thread_t *thread, void *obj)
{
	struct vid_helper *vh = obj;
	switch_channel_t *channel = switch_core_session_get_channel(vh->session_a);
	switch_status_t status;
	switch_frame_t *read_frame;

	vh->up = 1;
	while (switch_channel_ready(channel) && vh->up == 1) {
		status = switch_core_session_read_video_frame(vh->session_a, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		switch_core_session_write_video_frame(vh->session_b, read_frame, SWITCH_IO_FLAG_NONE, 0);

	}
	vh->up = 0;
	return NULL;
}

static void launch_video(struct vid_helper *vh)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, switch_core_session_get_pool(vh->session_a));
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, video_bridge_thread, vh, switch_core_session_get_pool(vh->session_a));
}
#endif

struct switch_ivr_bridge_data {
	switch_core_session_t *session;
	char b_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int stream_id;
	switch_input_callback_function_t input_callback;
	void *session_data;
};
typedef struct switch_ivr_bridge_data switch_ivr_bridge_data_t;

static void *audio_bridge_thread(switch_thread_t *thread, void *obj)
{
	switch_ivr_bridge_data_t *data = obj;
	int stream_id = 0, pre_b = 0, ans_a = 0, ans_b = 0, originator = 0;
	switch_input_callback_function_t input_callback;
	switch_core_session_message_t *message, msg = { 0 };
	void *user_data;
	switch_channel_t *chan_a, *chan_b;
	switch_frame_t *read_frame;
	switch_core_session_t *session_a, *session_b;
	uint32_t loop_count = 0;
	const char *app_name = NULL, *app_arg = NULL;
	const char *hook_var = NULL;
	int inner_bridge = 0;
	switch_codec_t silence_codec = { 0 };
	switch_frame_t silence_frame = { 0 };
	int16_t silence_data[SWITCH_RECOMMENDED_BUFFER_SIZE/2] = { 0 };
	const char *silence_var, *var;
	int silence_val = 0, bypass_media_after_bridge = 0;
#ifdef SWITCH_VIDEO_IN_THREADS
	struct vid_helper vh = { 0 };
	uint32_t vid_launch = 0;
#endif

	session_a = data->session;
	if (!(session_b = switch_core_session_locate(data->b_uuid))) {
		return NULL;
	}
	
	input_callback = data->input_callback;
	user_data = data->session_data;
	stream_id = data->stream_id;

	chan_a = switch_core_session_get_channel(session_a);
	chan_b = switch_core_session_get_channel(session_b);

	ans_a = switch_channel_test_flag(chan_a, CF_ANSWERED);
	if ((originator = switch_channel_test_flag(chan_a, CF_ORIGINATOR))) {
		pre_b = switch_channel_test_flag(chan_a, CF_EARLY_MEDIA);
		ans_b = switch_channel_test_flag(chan_b, CF_ANSWERED);
	}

	inner_bridge = switch_channel_test_flag(chan_a, CF_INNER_BRIDGE);


	switch_channel_set_flag(chan_a, CF_BRIDGED);


	switch_channel_wait_for_flag(chan_b, CF_BRIDGED, SWITCH_TRUE, 10000, chan_a);

	if (!switch_channel_test_flag(chan_b, CF_BRIDGED)) {
		switch_channel_hangup(chan_b, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto end_of_bridge_loop;
	}

	if ((var = switch_channel_get_variable(chan_a, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE)) && switch_true(var)) {
		bypass_media_after_bridge = 1;
		switch_channel_set_variable(chan_a, SWITCH_BYPASS_MEDIA_AFTER_BRIDGE_VARIABLE, NULL);
	}

	if ((silence_var = switch_channel_get_variable(chan_a, "bridge_generate_comfort_noise"))) {
		switch_codec_t *read_codec = NULL;
		
		if (!(read_codec = switch_core_session_get_read_codec(session_a))) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Channel has no media!\n");
			goto end_of_bridge_loop;
		}

		if (switch_true(silence_var)) {
			silence_val = 1400;
		} else {
			if ((silence_val = atoi(silence_var)) < 0) {
				silence_val = 0;
			}
		}

		if (silence_val) {
			if (switch_core_codec_init(&silence_codec,
									   "L16",
									   NULL, 
									   read_codec->implementation->actual_samples_per_second, 
									   read_codec->implementation->microseconds_per_frame / 1000,
									   1,
									   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, 
									   NULL, 
									   switch_core_session_get_pool(session_a)) != SWITCH_STATUS_SUCCESS) {
				
				silence_val = 0;
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setup generated silence from %s to %s at %d\n", switch_channel_get_name(chan_a), switch_channel_get_name(chan_b), silence_val);
				silence_frame.codec = &silence_codec;
				silence_frame.data = silence_data;
				silence_frame.buflen = sizeof(silence_data);
				silence_frame.datalen = read_codec->implementation->bytes_per_frame;
				silence_frame.samples = silence_frame.datalen / sizeof(int16_t);
			}
		}
	}
	
	for (;;) {
		switch_channel_state_t b_state;
		switch_status_t status;
		switch_event_t *event;
		loop_count++;

		if (!switch_channel_test_flag(chan_b, CF_BRIDGED)) {
			goto end_of_bridge_loop;
		}

		if (!switch_channel_ready(chan_a)) {
			goto end_of_bridge_loop;
		}

		if ((b_state = switch_channel_get_state(chan_b)) >= CS_HANGUP) {
			goto end_of_bridge_loop;
		}

		if (switch_channel_test_flag(chan_a, CF_TRANSFER) || switch_channel_test_flag(chan_b, CF_TRANSFER)) {
			switch_channel_clear_flag(chan_a, CF_HOLD);
			switch_channel_clear_flag(chan_a, CF_SUSPEND);
			goto end_of_bridge_loop;
		}

		if (loop_count > 50 && switch_core_session_private_event_count(session_a)) {
			switch_channel_set_flag(chan_b, CF_SUSPEND);
			msg.string_arg = data->b_uuid;
			msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
			msg.from = __FILE__;
			switch_core_session_receive_message(session_a, &msg);
			switch_ivr_parse_next_event(session_a);
			msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
			switch_core_session_receive_message(session_a, &msg);
			switch_channel_clear_flag(chan_b, CF_SUSPEND);
			switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
		}

		if (!inner_bridge && (switch_channel_test_flag(chan_a, CF_SUSPEND) || switch_channel_test_flag(chan_b, CF_SUSPEND))) {
			status = switch_core_session_read_frame(session_a, &read_frame, SWITCH_IO_FLAG_NONE, stream_id);

			if (!SWITCH_READ_ACCEPTABLE(status)) {
				goto end_of_bridge_loop;
			}
			continue;
		}
#ifdef SWITCH_VIDEO_IN_THREADS
		if (switch_channel_test_flag(chan_a, CF_VIDEO) && switch_channel_test_flag(chan_b, CF_VIDEO) && !vid_launch) {
			vid_launch++;
			vh.session_a = session_a;
			vh.session_b = session_b;
			launch_video(&vh);
		}
#endif

		if (loop_count > 50 && bypass_media_after_bridge) {
			switch_ivr_nomedia(switch_core_session_get_uuid(session_a), SMF_REBRIDGE);
			bypass_media_after_bridge = 0;
		}
		
		/* if 1 channel has DTMF pass it to the other */
		while (switch_channel_has_dtmf(chan_a)) {
			switch_dtmf_t dtmf = { 0, 0 };
			if (switch_channel_dequeue_dtmf(chan_a, &dtmf) == SWITCH_STATUS_SUCCESS) {
				int send_dtmf = 1;

				if (input_callback) {
					switch_status_t cb_status = input_callback(session_a, (void *) &dtmf, SWITCH_INPUT_TYPE_DTMF, user_data, 0);

					if (cb_status == SWITCH_STATUS_IGNORE) {
						send_dtmf = 0;
					} else if (cb_status != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s ended call via DTMF\n", switch_channel_get_name(chan_a));
						switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
						goto end_of_bridge_loop;
					}
				}

				if (send_dtmf) {
					switch_core_session_send_dtmf(session_b, &dtmf);
				}
			}
		}

		if (switch_core_session_dequeue_event(session_a, &event) == SWITCH_STATUS_SUCCESS) {
			if (input_callback) {
				status = input_callback(session_a, event, SWITCH_INPUT_TYPE_EVENT, user_data, 0);
			}

			if (event->event_id != SWITCH_EVENT_MESSAGE || switch_core_session_receive_event(session_b, &event) != SWITCH_STATUS_SUCCESS) {
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
				if (switch_channel_answer(chan_a) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Media Establishment Failed.\n", switch_channel_get_name(chan_a));
					goto end_of_bridge_loop;
				}
				ans_a++;
			} else if (!pre_b && switch_channel_test_flag(chan_b, CF_EARLY_MEDIA)) {
				if (switch_channel_pre_answer(chan_a) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Media Establishment Failed.\n", switch_channel_get_name(chan_a));
					goto end_of_bridge_loop;
				}
				pre_b++;
			}
			if (!pre_b) {
				switch_yield(10000);
				continue;
			}
		}
#ifndef SWITCH_VIDEO_IN_THREADS
		if (switch_channel_test_flag(chan_a, CF_VIDEO) && switch_channel_test_flag(chan_b, CF_VIDEO)) {
			/* read video from 1 channel and write it to the other */
			status = switch_core_session_read_video_frame(session_a, &read_frame, SWITCH_IO_FLAG_NONE, 0);

			if (!SWITCH_READ_ACCEPTABLE(status)) {
				goto end_of_bridge_loop;
			}

			switch_core_session_write_video_frame(session_b, read_frame, SWITCH_IO_FLAG_NONE, 0);
		}
#endif

		/* read audio from 1 channel and write it to the other */
		status = switch_core_session_read_frame(session_a, &read_frame, SWITCH_IO_FLAG_NONE, stream_id);

		if (SWITCH_READ_ACCEPTABLE(status)) {
			if (switch_test_flag(read_frame, SFF_CNG)) {
				if (silence_val) {
					switch_generate_sln_silence((int16_t *) silence_frame.data, silence_frame.samples, silence_val);
					read_frame = &silence_frame;
				} else {
					continue;
				}
			}
			
			if (status != SWITCH_STATUS_BREAK && !switch_channel_test_flag(chan_a, CF_HOLD)) {
				if (switch_core_session_write_frame(session_b, read_frame, SWITCH_IO_FLAG_NONE, stream_id) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, 
									  "%s ending bridge by request from write function\n", switch_channel_get_name(chan_b));
									  
					
					goto end_of_bridge_loop;
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s ending bridge by request from read function\n", switch_channel_get_name(chan_a));
			goto end_of_bridge_loop;
		}
	}

 end_of_bridge_loop:

	if (silence_val) {
		switch_core_codec_destroy(&silence_codec);
	}


#ifdef SWITCH_VIDEO_IN_THREADS
	if (vh.up) {
		vh.up = -1;
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Ending video thread.\n");
		while (vh.up) {
			switch_yield(100000);
		}
	}
#endif

	if (!inner_bridge) {
		hook_var = switch_channel_get_variable(chan_a, SWITCH_API_BRIDGE_END_VARIABLE);
	}

	if (!switch_strlen_zero(hook_var)) {
		switch_stream_handle_t stream = { 0 };
		char *cmd = switch_core_session_strdup(session_a, hook_var);
		char *arg = NULL;
		if ((arg = strchr(cmd, ' '))) {
			*arg++ = '\0';
		}
		SWITCH_STANDARD_STREAM(stream);
		switch_api_execute(cmd, arg, NULL, &stream);
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "\nPost-Bridge Command %s(%s):\n%s\n", cmd, arg, switch_str_nil((char *) stream.data));
		switch_safe_free(stream.data);
	}


	if (switch_channel_get_state(chan_b) >= CS_HANGUP) {
		if (originator && switch_channel_ready(chan_a) && !switch_channel_test_flag(chan_a, CF_ANSWERED)) {
			switch_channel_hangup(chan_a, switch_channel_get_cause(chan_b));
		}
	}

	msg.string_arg = data->b_uuid;
	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	msg.from = __FILE__;
	switch_core_session_receive_message(session_a, &msg);

	if (!inner_bridge && switch_channel_get_state(chan_a) < CS_HANGUP) {
		if ((app_name = switch_channel_get_variable(chan_a, SWITCH_EXEC_AFTER_BRIDGE_APP_VARIABLE))) {
			switch_caller_extension_t *extension = NULL;
			if ((extension = switch_caller_extension_new(session_a, app_name, app_name)) == 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "memory error!\n");
				goto end;
			}
			app_arg = switch_channel_get_variable(chan_a, SWITCH_EXEC_AFTER_BRIDGE_ARG_VARIABLE);

			switch_caller_extension_add_application(session_a, extension, (char *) app_name, app_arg);
			switch_channel_set_caller_extension(chan_a, extension);

			if (switch_channel_get_state(chan_a) == CS_EXECUTE) {
				switch_channel_set_flag(chan_a, CF_RESET);
			} else {
				switch_channel_set_state(chan_a, CS_EXECUTE);
			}
		}
	}

  end:

	switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
	switch_core_session_reset(session_a, SWITCH_TRUE);
	switch_channel_set_variable(chan_a, SWITCH_BRIDGE_VARIABLE, NULL);
	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "BRIDGE THREAD DONE [%s]\n", switch_channel_get_name(chan_a));
	if (!inner_bridge) {
		switch_channel_clear_flag(chan_a, CF_BRIDGED);
	}
	switch_core_session_rwunlock(session_b);
	return NULL;
}

static switch_status_t audio_bridge_on_exchange_media(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_ivr_bridge_data_t *bd = switch_channel_get_private(channel, "_bridge_");
	switch_channel_state_t state;

	if (bd) {
		switch_channel_set_private(channel, "_bridge_", NULL);
		if (bd->session == session && *bd->b_uuid) {
			audio_bridge_thread(NULL, (void *) bd);
		} else {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		}
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}
	switch_channel_clear_state_handler(channel, &audio_bridge_peer_state_handlers);

	state = switch_channel_get_state(channel);

	if (!switch_channel_test_flag(channel, CF_TRANSFER) && state != CS_PARK && state != CS_ROUTING && !switch_channel_test_flag(channel, CF_INNER_BRIDGE)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
	}

	if (switch_true(switch_channel_get_variable(channel, SWITCH_COPY_XML_CDR_VARIABLE))) {
		switch_xml_t cdr;
		char *xml_text;

		if (switch_ivr_generate_xml_cdr(session, &cdr) == SWITCH_STATUS_SUCCESS) {
			if ((xml_text = switch_xml_toxml(cdr, SWITCH_FALSE))) {
				switch_channel_set_variable_partner(channel, "b_leg_cdr", xml_text);
				switch_safe_free(xml_text);
			}
			switch_xml_free(cdr);
		}
	}


	return SWITCH_STATUS_FALSE;
}

static switch_status_t audio_bridge_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CUSTOM ROUTING\n", switch_channel_get_name(channel));

	/* put the channel in a passive state so we can loop audio to it */
	switch_channel_set_state(channel, CS_CONSUME_MEDIA);
	return SWITCH_STATUS_FALSE;
}

static switch_status_t audio_bridge_on_consume_media(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CUSTOM HOLD\n", switch_channel_get_name(channel));

	/* put the channel in a passive state so we can loop audio to it */
	return SWITCH_STATUS_FALSE;
}

static const switch_state_handler_table_t audio_bridge_peer_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ audio_bridge_on_routing,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ audio_bridge_on_exchange_media,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ audio_bridge_on_consume_media,
};

static switch_status_t uuid_bridge_on_reset(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CUSTOM RESET\n", switch_channel_get_name(channel));

	switch_channel_clear_flag(channel, CF_TRANSFER);

	if (switch_channel_test_flag(channel, CF_ORIGINATOR)) {
		switch_channel_set_state(channel, CS_SOFT_EXECUTE);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t uuid_bridge_on_soft_execute(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_core_session_t *other_session;
	const char *other_uuid = NULL;

	switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s CUSTOM TRANSMIT\n", switch_channel_get_name(channel));
	switch_channel_clear_state_handler(channel, NULL);

	switch_channel_clear_flag(channel, CF_TRANSFER);

	if (!switch_channel_test_flag(channel, CF_ORIGINATOR)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((other_uuid = switch_channel_get_variable(channel, SWITCH_UUID_BRIDGE)) && (other_session = switch_core_session_locate(other_uuid))) {
		switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
		switch_event_t *event;
		uint8_t ready_a, ready_b;
		switch_channel_state_t state;

		switch_channel_set_variable(channel, SWITCH_UUID_BRIDGE, NULL);

		switch_channel_wait_for_state(channel, other_channel, CS_RESET);

		if (switch_ivr_wait_for_answer(session, other_session) != SWITCH_STATUS_SUCCESS) {
			switch_core_session_rwunlock(other_session);
			switch_channel_hangup(channel, SWITCH_CAUSE_ORIGINATOR_CANCEL);
			return SWITCH_STATUS_FALSE;
		}

		if (switch_channel_get_state(other_channel) == CS_RESET) {
			switch_channel_set_state(other_channel, CS_SOFT_EXECUTE);
		}

		switch_channel_wait_for_state(channel, other_channel, CS_SOFT_EXECUTE);

		switch_channel_clear_flag(channel, CF_TRANSFER);
		switch_channel_clear_flag(other_channel, CF_TRANSFER);
		switch_core_session_reset(session, SWITCH_TRUE);
		switch_core_session_reset(other_session, SWITCH_TRUE);

		ready_a = switch_channel_ready(channel);
		ready_b = switch_channel_ready(other_channel);

		if (!ready_a || !ready_b) {
			if (!ready_a) {
				switch_channel_hangup(other_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}

			if (!ready_b) {
				const char *cid = switch_channel_get_variable(other_channel, "rdnis");
				if (ready_a && cid) {
					switch_ivr_session_transfer(session, cid, NULL, NULL);
				} else {
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}
			}
			switch_core_session_rwunlock(other_session);
			return SWITCH_STATUS_FALSE;
		}

		/* fire events that will change the data table from "show channels" */
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", "uuid_bridge");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Data", switch_core_session_get_uuid(other_session));
			switch_event_fire(&event);
		}

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(other_channel, event);
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", "uuid_bridge");
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Data", switch_core_session_get_uuid(session));
			switch_event_fire(&event);
		}

		switch_ivr_multi_threaded_bridge(session, other_session, NULL, NULL, NULL);

		state = switch_channel_get_state(channel);
		if (!switch_channel_test_flag(channel, CF_TRANSFER) && state < CS_HANGUP && state != CS_ROUTING && state != CS_PARK) {
			switch_channel_set_state(channel, CS_EXECUTE);
		}
		switch_core_session_rwunlock(other_session);
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}



	return SWITCH_STATUS_FALSE;
}

static const switch_state_handler_table_t uuid_bridge_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ uuid_bridge_on_soft_execute,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ NULL,
	/*.on_reset */ uuid_bridge_on_reset
};

static switch_status_t signal_bridge_on_hibernate(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);
	switch_channel_clear_flag(channel, CF_TRANSFER);

	switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE));

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t signal_bridge_on_hangup(switch_core_session_t *session)
{
	const char *uuid;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_core_session_t *other_session;
	switch_event_t *event;

	if (switch_channel_test_flag(channel, CF_ORIGINATOR)) {
		switch_channel_clear_flag(channel, CF_ORIGINATOR);
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
	}

	if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE))
		&& (other_session = switch_core_session_locate(uuid))) {
		switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
		const char *sbv = switch_channel_get_variable(other_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE);
		
		if (!switch_strlen_zero(sbv) && !strcmp(sbv, switch_core_session_get_uuid(session))) {
			
			switch_channel_set_variable(other_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);

			switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, NULL);
			switch_channel_set_variable(other_channel, SWITCH_BRIDGE_VARIABLE, NULL);

			if (switch_channel_get_state(other_channel) < CS_HANGUP) {
				switch_channel_hangup(other_channel, switch_channel_get_cause(channel));
			}
		}
		
		switch_channel_set_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);

		switch_core_session_rwunlock(other_session);
	}

	return SWITCH_STATUS_SUCCESS;
}

static const switch_state_handler_table_t signal_bridge_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ signal_bridge_on_hangup,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ NULL,
	/*.on_consume_media */ NULL,
	/*.on_hibernate */ signal_bridge_on_hibernate
};

SWITCH_DECLARE(switch_status_t) switch_ivr_signal_bridge(switch_core_session_t *session, switch_core_session_t *peer_session)
{
	switch_channel_t *caller_channel = switch_core_session_get_channel(session);
	switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
	switch_event_t *event;

	if (switch_channel_get_state(peer_channel) >= CS_HANGUP) {
		switch_channel_hangup(caller_channel, switch_channel_get_cause(peer_channel));
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_channel_ready(caller_channel)) {
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_ORIGINATOR_CANCEL);
		return SWITCH_STATUS_FALSE;
	}

	switch_channel_set_variable(caller_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, switch_core_session_get_uuid(peer_session));
	switch_channel_set_variable(peer_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));

	switch_channel_set_flag(caller_channel, CF_ORIGINATOR);

	switch_channel_clear_state_handler(caller_channel, NULL);
	switch_channel_clear_state_handler(peer_channel, NULL);

	switch_channel_add_state_handler(caller_channel, &signal_bridge_state_handlers);
	switch_channel_add_state_handler(peer_channel, &signal_bridge_state_handlers);

	/* fire events that will change the data table from "show channels" */
	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(caller_channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", "signal_bridge");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Data", switch_core_session_get_uuid(peer_session));
		switch_event_fire(&event);
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_EXECUTE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(peer_channel, event);
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application", "signal_bridge");
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Application-Data", switch_core_session_get_uuid(session));
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
	
	if (switch_channel_test_flag(caller_channel, CF_BRIDGED)) {
		switch_channel_set_flag(caller_channel, CF_TRANSFER);
		switch_channel_set_flag(peer_channel, CF_TRANSFER);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_multi_threaded_bridge(switch_core_session_t *session,
																 switch_core_session_t *peer_session,
																 switch_input_callback_function_t input_callback, void *session_data,
																 void *peer_session_data)
{
	switch_ivr_bridge_data_t *a_leg = switch_core_session_alloc(session, sizeof(*a_leg));
	switch_ivr_bridge_data_t *b_leg = switch_core_session_alloc(peer_session, sizeof(*b_leg));
	switch_channel_t *caller_channel = switch_core_session_get_channel(session);
	switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
	int stream_id = 0;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_state_t state;
	switch_event_t *event;
	int br = 0;
	int inner_bridge = switch_channel_test_flag(caller_channel, CF_INNER_BRIDGE);

	switch_channel_set_flag(caller_channel, CF_ORIGINATOR);
	switch_channel_clear_flag(caller_channel, CF_TRANSFER);
	switch_channel_clear_flag(peer_channel, CF_TRANSFER);

	b_leg->session = peer_session;
	switch_copy_string(b_leg->b_uuid, switch_core_session_get_uuid(session), sizeof(b_leg->b_uuid));
	b_leg->stream_id = stream_id;
	b_leg->input_callback = input_callback;
	b_leg->session_data = peer_session_data;

	a_leg->session = session;
	switch_copy_string(a_leg->b_uuid, switch_core_session_get_uuid(peer_session), sizeof(a_leg->b_uuid));
	a_leg->stream_id = stream_id;
	a_leg->input_callback = input_callback;
	a_leg->session_data = session_data;

	switch_channel_add_state_handler(peer_channel, &audio_bridge_peer_state_handlers);

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) && !switch_channel_test_flag(caller_channel, CF_ANSWERED)) {
		switch_channel_answer(caller_channel);
	}

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA) ||
		switch_channel_test_flag(peer_channel, CF_RING_READY)) {
		switch_core_session_message_t msg = { 0 };
		const char *app, *data;

		switch_channel_set_state(peer_channel, CS_CONSUME_MEDIA);

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(caller_channel, event);
			switch_event_fire(&event);
			br = 1;
		}

		if (switch_core_session_read_lock(peer_session) == SWITCH_STATUS_SUCCESS) {
			switch_channel_set_variable(caller_channel, SWITCH_BRIDGE_VARIABLE, switch_core_session_get_uuid(peer_session));
			switch_channel_set_variable(peer_channel, SWITCH_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));

			if (!switch_channel_media_ready(caller_channel) || 
				!(switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA))) {
				if ((status = switch_ivr_wait_for_answer(session, peer_session)) != SWITCH_STATUS_SUCCESS) {
					switch_channel_state_t w_state = switch_channel_get_state(caller_channel);
					switch_channel_hangup(peer_channel, SWITCH_CAUSE_ALLOTTED_TIMEOUT);
					if (w_state < CS_HANGUP && w_state != CS_ROUTING && w_state != CS_PARK && !switch_channel_test_flag(caller_channel, CF_TRANSFER) &&
						w_state != CS_EXECUTE) {
						const char *ext = switch_channel_get_variable(peer_channel, "original_destination_number");
						if (!ext) {
							ext = switch_channel_get_variable(peer_channel, "destination_number");
						}

						if (ext) {
							switch_ivr_session_transfer(session, ext, NULL, NULL);
						} else {
							switch_channel_hangup(caller_channel, SWITCH_CAUSE_ALLOTTED_TIMEOUT);
						}
					}
					switch_core_session_rwunlock(peer_session);
					goto done;
				}
				if (switch_channel_test_flag(peer_channel, CF_ANSWERED) && !switch_channel_test_flag(caller_channel, CF_ANSWERED)) {
					switch_channel_answer(caller_channel);
				}
			}

			msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
			msg.from = __FILE__;
			msg.string_arg = switch_core_session_strdup(peer_session, switch_core_session_get_uuid(session));

			if (switch_core_session_receive_message(peer_session, &msg) != SWITCH_STATUS_SUCCESS) {
				status = SWITCH_STATUS_FALSE;
				switch_core_session_rwunlock(peer_session);
				goto done;
			}

			msg.string_arg = switch_core_session_strdup(session, switch_core_session_get_uuid(peer_session));
			if (switch_core_session_receive_message(session, &msg) != SWITCH_STATUS_SUCCESS) {
				status = SWITCH_STATUS_FALSE;
				switch_core_session_rwunlock(peer_session);
				goto done;
			}

			switch_channel_set_variable(caller_channel, SWITCH_BRIDGE_CHANNEL_VARIABLE, switch_channel_get_name(peer_channel));
			switch_channel_set_variable(caller_channel, SWITCH_BRIDGE_UUID_VARIABLE, switch_core_session_get_uuid(peer_session));
			switch_channel_set_variable(caller_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(peer_session));
			switch_channel_set_variable(peer_channel, SWITCH_BRIDGE_CHANNEL_VARIABLE, switch_channel_get_name(caller_channel));
			switch_channel_set_variable(peer_channel, SWITCH_BRIDGE_UUID_VARIABLE, switch_core_session_get_uuid(session));
			switch_channel_set_variable(peer_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(session));

			if ((app = switch_channel_get_variable(caller_channel, "bridge_pre_execute_aleg_app"))) {
				data = switch_channel_get_variable(caller_channel, "bridge_pre_execute_aleg_data");
				switch_core_session_execute_application(session, app, data);
			}

			if ((app = switch_channel_get_variable(caller_channel, "bridge_pre_execute_bleg_app"))) {
				data = switch_channel_get_variable(caller_channel, "bridge_pre_execute_bleg_data");
				switch_core_session_execute_application(peer_session, app, data);
			}
			
			switch_channel_set_private(peer_channel, "_bridge_", b_leg);
			switch_channel_set_state(peer_channel, CS_EXCHANGE_MEDIA);
			audio_bridge_thread(NULL, (void *) a_leg);

			switch_channel_clear_flag(caller_channel, CF_ORIGINATOR);

			if (!switch_channel_test_flag(peer_channel, CF_TRANSFER) && switch_channel_get_state(peer_channel) == CS_EXCHANGE_MEDIA) {
				switch_channel_set_state(peer_channel, CS_RESET);
			}

			while (switch_channel_get_state(peer_channel) == CS_EXCHANGE_MEDIA) {
				switch_yield(1000);
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

	if (br && switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(caller_channel, event);
		switch_event_fire(&event);
	}

	state = switch_channel_get_state(caller_channel);

	if (!switch_channel_test_flag(caller_channel, CF_TRANSFER) && !inner_bridge) {
		if ((state != CS_EXECUTE && state != CS_SOFT_EXECUTE && state != CS_PARK && state != CS_ROUTING) ||
			(switch_channel_test_flag(peer_channel, CF_ANSWERED) && state < CS_HANGUP &&
			 switch_true(switch_channel_get_variable(caller_channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE)))) {
			switch_channel_hangup(caller_channel, switch_channel_get_cause(peer_channel));
		}
	}

	return status;
}

static void cleanup_proxy_mode(switch_core_session_t *session)
{
	switch_core_session_t *sbsession;

	switch_channel_t *channel = switch_core_session_get_channel(session);


	if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
		const char *sbv = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE);
		
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Restore media to %s\n", switch_channel_get_name(channel));
		switch_ivr_media(switch_core_session_get_uuid(session), SMF_IMMEDIATE);

		if (!switch_strlen_zero(sbv) && (sbsession = switch_core_session_locate(sbv))) {
			switch_channel_t *sbchannel = switch_core_session_get_channel(sbsession);
			switch_channel_hangup(sbchannel, SWITCH_CAUSE_ATTENDED_TRANSFER);
			switch_core_session_rwunlock(sbsession);
		}
	}

	switch_channel_set_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);
	switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, NULL);
	switch_channel_set_variable(channel, SWITCH_BRIDGE_UUID_VARIABLE, NULL);

}


SWITCH_DECLARE(switch_status_t) switch_ivr_uuid_bridge(const char *originator_uuid, const char *originatee_uuid)
{
	switch_core_session_t *originator_session, *originatee_session, *swap_session;
	switch_channel_t *originator_channel, *originatee_channel, *swap_channel;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_caller_profile_t *cp, *originator_cp, *originatee_cp;
	char *p;

	if ((originator_session = switch_core_session_locate(originator_uuid))) {
		if ((originatee_session = switch_core_session_locate(originatee_uuid))) {
			originator_channel = switch_core_session_get_channel(originator_session);
			originatee_channel = switch_core_session_get_channel(originatee_session);

			if (switch_channel_get_state(originator_channel) >= CS_HANGUP) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s is hungup refusing to bridge.\n", switch_channel_get_name(originatee_channel));
				switch_core_session_rwunlock(originator_session);
				switch_core_session_rwunlock(originatee_session);
				return SWITCH_STATUS_FALSE;
			}

			if (!switch_channel_test_flag(originator_channel, CF_ANSWERED)) {
				if (switch_channel_test_flag(originatee_channel, CF_ANSWERED)) {
					swap_session = originator_session;
					originator_session = originatee_session;
					originatee_session = swap_session;

					swap_channel = originator_channel;
					originator_channel = originatee_channel;
					originatee_channel = swap_channel;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "reversing order of channels so this will work!\n");
				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_CRIT, "Neither channel is answered, cannot bridge them.\n");
					switch_core_session_rwunlock(originator_session);
					switch_core_session_rwunlock(originatee_session);
					return SWITCH_STATUS_FALSE;
				}
			}

			cleanup_proxy_mode(originator_session);
			cleanup_proxy_mode(originatee_session);
			
			/* override transmit state for originator_channel to bridge to originatee_channel 
			 * install pointer to originatee_session into originator_channel
			 * set CF_TRANSFER on both channels and change state to CS_SOFT_EXECUTE to
			 * inturrupt anything they are already doing.
			 * originatee_session will fall asleep and originator_session will bridge to it
			 */

			switch_channel_clear_state_handler(originator_channel, NULL);
			switch_channel_clear_state_handler(originatee_channel, NULL);
			switch_channel_set_state_flag(originator_channel, CF_ORIGINATOR);
			switch_channel_clear_flag(originatee_channel, CF_ORIGINATOR);
			switch_channel_add_state_handler(originator_channel, &uuid_bridge_state_handlers);
			switch_channel_add_state_handler(originatee_channel, &uuid_bridge_state_handlers);
			switch_channel_set_variable(originator_channel, SWITCH_UUID_BRIDGE, switch_core_session_get_uuid(originatee_session));

			switch_channel_set_variable(originator_channel, SWITCH_BRIDGE_CHANNEL_VARIABLE, switch_channel_get_name(originatee_channel));
			switch_channel_set_variable(originator_channel, SWITCH_BRIDGE_UUID_VARIABLE, switch_core_session_get_uuid(originatee_session));
			switch_channel_set_variable(originator_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(originatee_session));
			switch_channel_set_variable(originatee_channel, SWITCH_BRIDGE_CHANNEL_VARIABLE, switch_channel_get_name(originator_channel));
			switch_channel_set_variable(originatee_channel, SWITCH_BRIDGE_UUID_VARIABLE, switch_core_session_get_uuid(originator_session));
			switch_channel_set_variable(originatee_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(originator_session));

			
			originator_cp = switch_channel_get_caller_profile(originator_channel);
			originatee_cp = switch_channel_get_caller_profile(originatee_channel);


			switch_channel_set_variable(originatee_channel, "original_destination_number", originatee_cp->destination_number);
			switch_channel_set_variable(originatee_channel, "original_caller_id_name", originatee_cp->caller_id_name);
			switch_channel_set_variable(originatee_channel, "original_caller_id_number", originatee_cp->caller_id_number);

			switch_channel_set_variable(originator_channel, "original_destination_number", originator_cp->destination_number);
			switch_channel_set_variable(originator_channel, "original_caller_id_name", originator_cp->caller_id_name);
			switch_channel_set_variable(originator_channel, "original_caller_id_number", originator_cp->caller_id_number);

			cp = switch_caller_profile_clone(originatee_session, originatee_cp);
			cp->destination_number = switch_core_strdup(cp->pool, originator_cp->caller_id_number);
			cp->caller_id_number = switch_core_strdup(cp->pool, originator_cp->caller_id_number);
			cp->caller_id_name = switch_core_strdup(cp->pool, originator_cp->caller_id_name);
			cp->rdnis = switch_core_strdup(cp->pool, originatee_cp->destination_number);
			if ((p = strchr(cp->rdnis, '@'))) {
				*p = '\0';
			}
			switch_channel_set_caller_profile(originatee_channel, cp);
			switch_channel_set_originator_caller_profile(originatee_channel, switch_caller_profile_clone(originatee_session, originator_cp));

			cp = switch_caller_profile_clone(originator_session, originator_cp);
			cp->destination_number = switch_core_strdup(cp->pool, originatee_cp->caller_id_number);
			cp->caller_id_number = switch_core_strdup(cp->pool, originatee_cp->caller_id_number);
			cp->caller_id_name = switch_core_strdup(cp->pool, originatee_cp->caller_id_name);
			cp->rdnis = switch_core_strdup(cp->pool, originator_cp->destination_number);
			if ((p = strchr(cp->rdnis, '@'))) {
				*p = '\0';
			}
			switch_channel_set_caller_profile(originator_channel, cp);
			switch_channel_set_originatee_caller_profile(originator_channel, switch_caller_profile_clone(originator_session, originatee_cp));

			switch_channel_stop_broadcast(originator_channel);
			switch_channel_stop_broadcast(originatee_channel);

			switch_channel_set_flag(originator_channel, CF_TRANSFER);
			switch_channel_set_flag(originatee_channel, CF_TRANSFER);

			switch_channel_clear_flag(originator_channel, CF_ORIGINATING);
			switch_channel_clear_flag(originatee_channel, CF_ORIGINATING);

			/* change the states and let the chips fall where they may */
			switch_channel_set_state(originator_channel, CS_RESET);
			switch_channel_set_state(originatee_channel, CS_RESET);

			status = SWITCH_STATUS_SUCCESS;

			/* release the read locks we have on the channels */
			switch_core_session_rwunlock(originator_session);
			switch_core_session_rwunlock(originatee_session);

		} else {
			switch_core_session_rwunlock(originator_session);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "originatee uuid %s is not present\n", originatee_uuid);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "originator uuid %s is not present\n", originator_uuid);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_find_bridged_uuid(const char *uuid, char *b_uuid, switch_size_t blen)
{
	switch_core_session_t *rsession;
	switch_status_t status = SWITCH_STATUS_FALSE;

	switch_assert(uuid);

	if ((rsession = switch_core_session_locate(uuid))) {
		switch_channel_t *rchannel = switch_core_session_get_channel(rsession);
		const char *brto;

		if ((brto = switch_channel_get_variable(rchannel, SWITCH_SIGNAL_BOND_VARIABLE))) {
			switch_copy_string(b_uuid, brto, blen);
			status = SWITCH_STATUS_SUCCESS;
		}
		switch_core_session_rwunlock(rsession);
	}

	return status;

}

SWITCH_DECLARE(void) switch_ivr_intercept_session(switch_core_session_t *session, const char *uuid, switch_bool_t bleg)
{
	switch_core_session_t *rsession, *bsession = NULL;
	switch_channel_t *channel, *rchannel, *bchannel = NULL;
	const char *buuid;
	char brto[SWITCH_UUID_FORMATTED_LENGTH + 1] = "";

	if (bleg) {
		if (switch_ivr_find_bridged_uuid(uuid, brto, sizeof(brto)) == SWITCH_STATUS_SUCCESS) {
			uuid = switch_core_session_strdup(session, brto);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no uuid bridged to %s\n", uuid);
			return;
		}
	}

	if (switch_strlen_zero(uuid) || !(rsession = switch_core_session_locate(uuid))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "no uuid %s\n", uuid);
		return;
	}

	channel = switch_core_session_get_channel(session);
	rchannel = switch_core_session_get_channel(rsession);

	switch_channel_pre_answer(channel);

	if ((buuid = switch_channel_get_variable(rchannel, SWITCH_SIGNAL_BOND_VARIABLE))) {
		bsession = switch_core_session_locate(buuid);
		bchannel = switch_core_session_get_channel(bsession);
	}

	if (!switch_channel_test_flag(rchannel, CF_ANSWERED)) {
		switch_channel_answer(rchannel);
	}

	switch_channel_set_state_flag(rchannel, CF_TRANSFER);
	switch_channel_set_state(rchannel, CS_PARK);
	
	if (bchannel) {
		switch_channel_set_state_flag(bchannel, CF_TRANSFER);
		switch_channel_set_state(bchannel, CS_PARK);
	}

	switch_ivr_uuid_bridge(switch_core_session_get_uuid(session), uuid);
	switch_core_session_rwunlock(rsession);

	if (bsession) {
		switch_channel_hangup(bchannel, SWITCH_CAUSE_PICKED_OFF);
		switch_core_session_rwunlock(bsession);
	}

	

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
