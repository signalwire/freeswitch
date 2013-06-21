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
 *
 * switch_ivr_bridge.c -- IVR Library 
 *
 */

#include <switch.h>
#define DEFAULT_LEAD_FRAMES 10

static const switch_state_handler_table_t audio_bridge_peer_state_handlers;
static void cleanup_proxy_mode_a(switch_core_session_t *session);
static void cleanup_proxy_mode_b(switch_core_session_t *session);

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
	switch_channel_t *b_channel = switch_core_session_get_channel(vh->session_b);
	switch_status_t status;
	switch_frame_t *read_frame = 0;
	const char *source = switch_channel_get_variable(channel, "source");
	const char *b_source = switch_channel_get_variable(b_channel, "source");

	vh->up = 1;

	switch_core_session_read_lock(vh->session_a);
	switch_core_session_read_lock(vh->session_b);

	if (!switch_stristr("loopback", source) && !switch_stristr("loopback", b_source)) {
		switch_channel_set_flag(channel, CF_VIDEO_PASSIVE);
		switch_channel_set_flag(b_channel, CF_VIDEO_PASSIVE);
	}

	switch_core_session_refresh_video(vh->session_a);
	switch_core_session_refresh_video(vh->session_b);

	while (switch_channel_up_nosig(channel) && switch_channel_up_nosig(b_channel) && vh->up == 1) {

		if (switch_channel_media_up(channel)) {
			status = switch_core_session_read_video_frame(vh->session_a, &read_frame, SWITCH_IO_FLAG_NONE, 0);
			
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				switch_cond_next();
				continue;
			}
		}

		if (switch_test_flag(read_frame, SFF_CNG)) {
			continue;
		}

		if (switch_channel_media_up(b_channel)) {
			if (switch_core_session_write_video_frame(vh->session_b, read_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
				switch_cond_next();
				continue;
			}
		}

	}

	switch_channel_clear_flag(channel, CF_VIDEO_PASSIVE);
	switch_channel_clear_flag(b_channel, CF_VIDEO_PASSIVE);

	switch_core_session_kill_channel(vh->session_b, SWITCH_SIG_BREAK);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(vh->session_a), SWITCH_LOG_DEBUG, "%s video thread ended.\n", switch_channel_get_name(channel));

	switch_core_session_refresh_video(vh->session_a);
	switch_core_session_refresh_video(vh->session_b);

	switch_core_session_rwunlock(vh->session_a);
	switch_core_session_rwunlock(vh->session_b);

	vh->up = 0;
	return NULL;
}

static switch_thread_t *launch_video(struct vid_helper *vh)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, switch_core_session_get_pool(vh->session_a));
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, video_bridge_thread, vh, switch_core_session_get_pool(vh->session_a));
	return thread;
}
#endif


static void send_display(switch_core_session_t *session, switch_core_session_t *peer_session)
{

	switch_core_session_message_t *msg;
	switch_caller_profile_t *caller_profile, *peer_caller_profile;
	switch_channel_t *caller_channel, *peer_channel;
	const char *name, *number, *p;

	caller_channel = switch_core_session_get_channel(session);
	peer_channel = switch_core_session_get_channel(peer_session);

	caller_profile = switch_channel_get_caller_profile(caller_channel);
	peer_caller_profile = switch_channel_get_caller_profile(peer_channel);

	if (switch_channel_test_flag(caller_channel, CF_BRIDGE_ORIGINATOR)) {
		if (!zstr(peer_caller_profile->caller_id_name)) {
			name = peer_caller_profile->caller_id_name;
		} else {
			name = caller_profile->caller_id_name;
		}

		if (!zstr(peer_caller_profile->caller_id_number)) {
			number = peer_caller_profile->caller_id_number;
		} else {
			number = caller_profile->caller_id_number;		
		}

		if (zstr(number)) {
			number = "UNKNOWN";
		}
	} else {
		name = caller_profile->callee_id_name;
		number = caller_profile->callee_id_number;

		if (zstr(number)) {
			number = caller_profile->destination_number;
		}
	}

	
	if (zstr(name)) {
		name = number;
	}

	if ((p = strrchr(number, '/'))) {
		number = p + 1;
	}
	if ((p = strrchr(name, '/'))) {
		name = p + 1;
	}

	msg = switch_core_session_alloc(peer_session, sizeof(*msg));
	MESSAGE_STAMP_FFL(msg);
	msg->message_id = SWITCH_MESSAGE_INDICATE_DISPLAY;
	msg->string_array_arg[0] = switch_core_session_strdup(peer_session, name);
	msg->string_array_arg[1] = switch_core_session_strdup(peer_session, number);
	msg->from = __FILE__;
	switch_core_session_queue_message(peer_session, msg);

}


SWITCH_DECLARE(void) switch_ivr_bridge_display(switch_core_session_t *session, switch_core_session_t *peer_session)
{

	send_display(session, peer_session);
	send_display(peer_session, session);

}

struct switch_ivr_bridge_data {
	switch_core_session_t *session;
	char b_uuid[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int stream_id;
	switch_input_callback_function_t input_callback;
	void *session_data;
	int clean_exit;
};
typedef struct switch_ivr_bridge_data switch_ivr_bridge_data_t;

static void *audio_bridge_thread(switch_thread_t *thread, void *obj)
{
	switch_ivr_bridge_data_t *data = obj;
	int stream_id = 0, pre_b = 0, ans_a = 0, ans_b = 0, originator = 0;
	switch_input_callback_function_t input_callback;
	switch_core_session_message_t msg = { 0 };
	void *user_data;
	switch_channel_t *chan_a, *chan_b;
	switch_frame_t *read_frame;
	switch_core_session_t *session_a, *session_b;
	uint32_t read_frame_count = 0;
	const char *app_name = NULL, *app_arg = NULL;
	int inner_bridge = 0, exec_check = 0;
	switch_codec_t silence_codec = { 0 };
	switch_frame_t silence_frame = { 0 };
	int16_t silence_data[SWITCH_RECOMMENDED_BUFFER_SIZE / 2] = { 0 };
	const char *silence_var;
	int silence_val = 0, bypass_media_after_bridge = 0;
	const char *bridge_answer_timeout = NULL;
	int answer_timeout, sent_update = 0;
	time_t answer_limit = 0;
	const char *exec_app = NULL;
	const char *exec_data = NULL;

#ifdef SWITCH_VIDEO_IN_THREADS
	switch_thread_t *vid_thread = NULL;
	struct vid_helper vh = { 0 };
	uint32_t vid_launch = 0;
#endif
	data->clean_exit = 0;

	session_a = data->session;
	if (!(session_b = switch_core_session_locate(data->b_uuid))) {
		return NULL;
	}

	input_callback = data->input_callback;
	user_data = data->session_data;
	stream_id = data->stream_id;

	chan_a = switch_core_session_get_channel(session_a);
	chan_b = switch_core_session_get_channel(session_b);
	

	if ((exec_app = switch_channel_get_variable(chan_a, "bridge_pre_execute_app"))) {
		exec_data = switch_channel_get_variable(chan_a, "bridge_pre_execute_data");
	}

	bypass_media_after_bridge = switch_channel_test_flag(chan_a, CF_BYPASS_MEDIA_AFTER_BRIDGE);
	switch_channel_clear_flag(chan_a, CF_BYPASS_MEDIA_AFTER_BRIDGE);

	ans_a = switch_channel_test_flag(chan_a, CF_ANSWERED);

	if ((originator = switch_channel_test_flag(chan_a, CF_BRIDGE_ORIGINATOR))) {
		pre_b = switch_channel_test_flag(chan_a, CF_EARLY_MEDIA);
		ans_b = switch_channel_test_flag(chan_b, CF_ANSWERED);
	}

	inner_bridge = switch_channel_test_flag(chan_a, CF_INNER_BRIDGE);
	
	if (!switch_channel_test_flag(chan_a, CF_ANSWERED) && (bridge_answer_timeout = switch_channel_get_variable(chan_a, "bridge_answer_timeout"))) {
		if ((answer_timeout = atoi(bridge_answer_timeout)) < 0) {
			answer_timeout = 0;
		} else {
			answer_limit = switch_epoch_time_now(NULL) + answer_timeout;
		}
	}

	switch_channel_clear_flag(chan_a, CF_INTERCEPT);
	switch_channel_clear_flag(chan_a, CF_INTERCEPTED);

	switch_channel_set_flag(chan_a, CF_BRIDGED);

	switch_channel_wait_for_flag(chan_b, CF_BRIDGED, SWITCH_TRUE, 10000, chan_a);

	if (!switch_channel_test_flag(chan_b, CF_BRIDGED)) {
		if (!(switch_channel_test_flag(chan_b, CF_TRANSFER) || switch_channel_test_flag(chan_b, CF_REDIRECT)
			  || switch_channel_get_state(chan_b) == CS_RESET)) {
			switch_channel_hangup(chan_b, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		}
		goto end_of_bridge_loop;
	}

	if (bypass_media_after_bridge) {
		const char *source_a = switch_channel_get_variable(chan_a, "source");
		const char *source_b = switch_channel_get_variable(chan_b, "source");

		if (!source_a) source_a = "";
		if (!source_b) source_b = "";

		if (switch_stristr("loopback", source_a) || switch_stristr("loopback", source_b)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_WARNING, "Cannot bypass media while bridged to a loopback address.\n");
			bypass_media_after_bridge = 0;
		}
	}

	if ((silence_var = switch_channel_get_variable(chan_a, "bridge_generate_comfort_noise"))) {
		switch_codec_implementation_t read_impl = { 0 };
		switch_core_session_get_read_impl(session_a, &read_impl);

		if (!switch_channel_media_up(chan_a)) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_ERROR, "Channel has no media!\n");
			goto end_of_bridge_loop;
		}

		if (switch_true(silence_var)) {
			silence_val = 1400;
		} else {
			if ((silence_val = atoi(silence_var)) < -1) {
				silence_val = 0;
			}
		}

		if (silence_val) {
			if (switch_core_codec_init(&silence_codec,
									   "L16",
									   NULL,
									   read_impl.actual_samples_per_second,
									   read_impl.microseconds_per_packet / 1000,
									   1,
									   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
									   NULL, switch_core_session_get_pool(session_a)) != SWITCH_STATUS_SUCCESS) {

				silence_val = 0;
			} else {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "Setup generated silence from %s to %s at %d\n", switch_channel_get_name(chan_a),
								  switch_channel_get_name(chan_b), silence_val);
				silence_frame.codec = &silence_codec;
				silence_frame.data = silence_data;
				silence_frame.buflen = sizeof(silence_data);
				silence_frame.datalen = read_impl.decoded_bytes_per_packet;
				silence_frame.samples = silence_frame.datalen / sizeof(int16_t);
			}
		}
	}

	for (;;) {
		switch_channel_state_t b_state;
		switch_status_t status;
		switch_event_t *event;

		if (switch_channel_test_flag(chan_a, CF_TRANSFER)) {
			data->clean_exit = 1;
		}

		if (data->clean_exit || switch_channel_test_flag(chan_b, CF_TRANSFER)) {
			switch_channel_clear_flag(chan_a, CF_HOLD);
			switch_channel_clear_flag(chan_a, CF_SUSPEND);
			goto end_of_bridge_loop;
		}

		if (!switch_channel_test_flag(chan_b, CF_BRIDGED)) {
			goto end_of_bridge_loop;
		}

		if (!switch_channel_ready(chan_a)) {
			goto end_of_bridge_loop;
		}

		if ((b_state = switch_channel_down_nosig(chan_b))) {
			goto end_of_bridge_loop;
		}

		if (switch_channel_test_flag(chan_a, CF_HOLD_ON_BRIDGE)) {
			switch_core_session_message_t hmsg = { 0 };
			switch_channel_clear_flag(chan_a, CF_HOLD_ON_BRIDGE);
			hmsg.message_id = SWITCH_MESSAGE_INDICATE_HOLD;
			hmsg.from = __FILE__;
			hmsg.numeric_arg = 1;
			switch_core_session_receive_message(session_a, &hmsg);
		}

		if (read_frame_count > DEFAULT_LEAD_FRAMES && switch_channel_media_ack(chan_a) && switch_core_session_private_event_count(session_a)) {
			switch_channel_set_flag(chan_b, CF_SUSPEND);
			msg.numeric_arg = 42;
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

		switch_ivr_parse_all_messages(session_a);

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
			vid_thread = launch_video(&vh);
		}
#endif

		if (read_frame_count >= DEFAULT_LEAD_FRAMES && switch_channel_media_ack(chan_a)) {

			if (!exec_check) {
				switch_channel_execute_on(chan_a, SWITCH_CHANNEL_EXECUTE_ON_PRE_BRIDGE_VARIABLE);

				if (!inner_bridge) {
					switch_channel_api_on(chan_a, SWITCH_API_BRIDGE_START_VARIABLE);
				}
				exec_check = 1;
			}
			
			if (exec_app) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "%s Bridge execute app %s(%s)\n", 
								  switch_channel_get_name(chan_a), exec_app, exec_data);

				switch_core_session_execute_application_async(session_a, exec_app, exec_data);
				exec_app = exec_data = NULL;
			}
			
			if ((bypass_media_after_bridge || switch_channel_test_flag(chan_b, CF_BYPASS_MEDIA_AFTER_BRIDGE)) && switch_channel_test_flag(chan_a, CF_ANSWERED)
				&& switch_channel_test_flag(chan_b, CF_ANSWERED)) {
				switch_ivr_nomedia(switch_core_session_get_uuid(session_a), SMF_REBRIDGE);
				bypass_media_after_bridge = 0;
				switch_channel_clear_flag(chan_b, CF_BYPASS_MEDIA_AFTER_BRIDGE);
				goto end_of_bridge_loop;
			}
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
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "%s ended call via DTMF\n", switch_channel_get_name(chan_a));
						switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
						goto end_of_bridge_loop;
					}
				}

				if (send_dtmf) {
					switch_core_session_send_dtmf(session_b, &dtmf);
					switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
				}
			}
		}

		if (switch_core_session_dequeue_event(session_a, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			if (input_callback) {
				status = input_callback(session_a, event, SWITCH_INPUT_TYPE_EVENT, user_data, 0);
			}

			if ((event->event_id != SWITCH_EVENT_COMMAND && event->event_id != SWITCH_EVENT_MESSAGE)
				|| switch_core_session_receive_event(session_b, &event) != SWITCH_STATUS_SUCCESS) {
				switch_event_destroy(&event);
			}

		}

		if (!switch_channel_test_flag(chan_a, CF_ANSWERED) && answer_limit && switch_epoch_time_now(NULL) > answer_limit) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "Answer timeout hit on %s.\n", switch_channel_get_name(chan_a));
			switch_channel_hangup(chan_a, SWITCH_CAUSE_ALLOTTED_TIMEOUT);
		}

		if (!switch_channel_test_flag(chan_a, CF_ANSWERED)) {
			if (originator) {
				if (!ans_b && switch_channel_test_flag(chan_b, CF_ANSWERED)) {
					switch_channel_pass_callee_id(chan_b, chan_a);
					if (switch_channel_answer(chan_a) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "%s Media Establishment Failed.\n", switch_channel_get_name(chan_a));
						goto end_of_bridge_loop;
					}
					ans_a = 1;
				} else if (!pre_b && switch_channel_test_flag(chan_b, CF_EARLY_MEDIA)) {
					if (switch_channel_pre_answer(chan_a) != SWITCH_STATUS_SUCCESS) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "%s Media Establishment Failed.\n", switch_channel_get_name(chan_a));
						goto end_of_bridge_loop;
					}
					pre_b = 1;
				}
				if (!pre_b) {
					switch_yield(10000);
					continue;
				}
			} else {
				ans_a = switch_channel_test_flag(chan_b, CF_ANSWERED);
			}
		}

		if (ans_a != ans_b) {
			switch_channel_t *un = ans_a ? chan_b : chan_a;
			switch_channel_t *a = un == chan_b ? chan_a : chan_b;

			if (switch_channel_direction(un) == SWITCH_CALL_DIRECTION_INBOUND) {
				if (switch_channel_direction(a) == SWITCH_CALL_DIRECTION_OUTBOUND || (un == chan_a && !originator)) {
					switch_channel_pass_callee_id(a, un);
				}

				if (switch_channel_answer(un) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "%s Media Establishment Failed.\n", switch_channel_get_name(un));
					goto end_of_bridge_loop;
				}

				if (ans_a) {
					ans_b = 1;
				} else {
					ans_a = 1;
				}
			}
		}

		if (originator && !ans_b) ans_b = switch_channel_test_flag(chan_b, CF_ANSWERED);

		if (originator && !sent_update && ans_a && ans_b && switch_channel_media_ack(chan_a) && switch_channel_media_ack(chan_b)) {
			switch_ivr_bridge_display(session_a, session_b);
			sent_update = 1;
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
			read_frame_count++;
			if (switch_test_flag(read_frame, SFF_CNG)) {
				if (silence_val) {
					switch_generate_sln_silence((int16_t *) silence_frame.data, silence_frame.samples, silence_val);
					read_frame = &silence_frame;
				} else if (!switch_channel_test_flag(chan_b, CF_ACCEPT_CNG)) {
					continue;
				}
			}

			if (switch_channel_test_flag(chan_a, CF_BRIDGE_NOWRITE)) {
				continue;
			}

			if (status != SWITCH_STATUS_BREAK && !switch_channel_test_flag(chan_a, CF_HOLD)) {
				if (switch_core_session_write_frame(session_b, read_frame, SWITCH_IO_FLAG_NONE, stream_id) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG,
									  "%s ending bridge by request from write function\n", switch_channel_get_name(chan_b));
					goto end_of_bridge_loop;
				}
			}
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "%s ending bridge by request from read function\n", switch_channel_get_name(chan_a));
			goto end_of_bridge_loop;
		}
	}

  end_of_bridge_loop:

#ifdef SWITCH_VIDEO_IN_THREADS
	if (vid_thread) {
		vh.up = -1;
		switch_channel_set_flag(chan_a, CF_NOT_READY);
		switch_channel_set_flag(chan_b, CF_NOT_READY);
		switch_core_session_kill_channel(session_a, SWITCH_SIG_BREAK);
		switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "Ending video thread.\n");
	}
#endif


	if (silence_val) {
		switch_core_codec_destroy(&silence_codec);
	}

	switch_channel_execute_on(chan_a, SWITCH_CHANNEL_EXECUTE_ON_POST_BRIDGE_VARIABLE);

	if (!inner_bridge) {
		switch_channel_api_on(chan_a, SWITCH_API_BRIDGE_END_VARIABLE);
	}

	if (!inner_bridge && switch_channel_up_nosig(chan_a)) {
		if ((app_name = switch_channel_get_variable(chan_a, SWITCH_EXEC_AFTER_BRIDGE_APP_VARIABLE))) {
			switch_caller_extension_t *extension = NULL;
			if ((extension = switch_caller_extension_new(session_a, app_name, app_name)) == 0) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_CRIT, "memory error!\n");
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

#ifdef SWITCH_VIDEO_IN_THREADS
	if (vid_thread) {
		switch_status_t st;

		if (vh.up) {
			switch_core_session_kill_channel(session_a, SWITCH_SIG_BREAK);
			switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "Ending video thread.\n");
		switch_thread_join(&st, vid_thread);
		switch_channel_clear_flag(chan_a, CF_NOT_READY);
		switch_channel_clear_flag(chan_b, CF_NOT_READY);
	}
#endif



	switch_core_session_reset(session_a, SWITCH_TRUE, SWITCH_TRUE);
	switch_channel_set_variable(chan_a, SWITCH_BRIDGE_VARIABLE, NULL);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session_a), SWITCH_LOG_DEBUG, "BRIDGE THREAD DONE [%s]\n", switch_channel_get_name(chan_a));
	switch_channel_clear_flag(chan_a, CF_BRIDGED);

	if (switch_channel_test_flag(chan_a, CF_LEG_HOLDING) && switch_channel_ready(chan_b) && switch_channel_get_state(chan_b) != CS_PARK) {
		const char *ext = switch_channel_get_variable(chan_a, "hold_hangup_xfer_exten");

		switch_channel_stop_broadcast(chan_b);

		if (zstr(ext)) {
			switch_call_cause_t cause = switch_channel_get_cause(chan_b);
			if (cause == SWITCH_CAUSE_NONE) {
				cause = SWITCH_CAUSE_NORMAL_CLEARING;
			}
			switch_channel_hangup(chan_b, cause);
		} else {
			switch_channel_set_variable(chan_b, SWITCH_TRANSFER_AFTER_BRIDGE_VARIABLE, ext);
		}
		switch_channel_clear_flag(chan_a, CF_LEG_HOLDING);
	}

	if (switch_channel_test_flag(chan_a, CF_INTERCEPTED)) {
		switch_channel_set_flag(chan_b, CF_INTERCEPT);
	}


	switch_core_session_kill_channel(session_b, SWITCH_SIG_BREAK);
	switch_core_session_rwunlock(session_b);
	return NULL;
}

static void transfer_after_bridge(switch_core_session_t *session, const char *where)
{
	int argc;
	char *argv[4] = { 0 };
	char *mydata;

	switch_channel_set_variable(switch_core_session_get_channel(session), SWITCH_TRANSFER_AFTER_BRIDGE_VARIABLE, NULL);

	if (!zstr(where) && (mydata = switch_core_session_strdup(session, where))) {
		if ((argc = switch_separate_string(mydata, ':', argv, (sizeof(argv) / sizeof(argv[0])))) >= 1) {
			switch_ivr_session_transfer(session, argv[0], argv[1], argv[2]);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No extension specified.\n");
		}
	}
}

static switch_status_t audio_bridge_on_exchange_media(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_ivr_bridge_data_t *bd = switch_channel_get_private(channel, "_bridge_");
	switch_channel_state_t state;
	const char *var;

	if (bd) {
		switch_channel_set_private(channel, "_bridge_", NULL);
		if (bd->session == session && *bd->b_uuid) {
			audio_bridge_thread(NULL, (void *) bd);
			switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);
		} else {
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		}
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}
	switch_channel_clear_state_handler(channel, &audio_bridge_peer_state_handlers);

	state = switch_channel_get_state(channel);

	if (state < CS_HANGUP && switch_true(switch_channel_get_variable(channel, SWITCH_PARK_AFTER_BRIDGE_VARIABLE))) {
		switch_ivr_park_session(session);
	} else if (state < CS_HANGUP && (var = switch_channel_get_variable(channel, SWITCH_TRANSFER_AFTER_BRIDGE_VARIABLE))) {
		transfer_after_bridge(session, var);
	} else {
		if (!switch_channel_test_flag(channel, CF_TRANSFER) && !switch_channel_test_flag(channel, CF_REDIRECT) &&
			!switch_channel_test_flag(channel, CF_XFER_ZOMBIE) && bd && !bd->clean_exit
			&& state != CS_PARK && state != CS_ROUTING && state == CS_EXCHANGE_MEDIA && !switch_channel_test_flag(channel, CF_INNER_BRIDGE)) {
			if (switch_channel_test_flag(channel, CF_INTERCEPTED)) {
				switch_channel_clear_flag(channel, CF_INTERCEPT);
				switch_channel_clear_flag(channel, CF_INTERCEPTED);
				return SWITCH_STATUS_FALSE;
			} else {
				if (switch_channel_test_flag(channel, CF_INTERCEPT)) {
					switch_channel_hangup(channel, SWITCH_CAUSE_PICKED_OFF);
				} else {
					if (!switch_channel_test_flag(channel, CF_ANSWERED)) {
						switch_channel_hangup(channel, SWITCH_CAUSE_ORIGINATOR_CANCEL);
					} else {
						switch_channel_hangup(channel, SWITCH_CAUSE_NORMAL_CLEARING);
					}
				}
			}
		}
	}

	if (switch_channel_get_state(channel) == CS_EXCHANGE_MEDIA) {
		switch_channel_set_variable(channel, "park_timeout", "3");
		switch_channel_set_state(channel, CS_PARK);
	}

	return SWITCH_STATUS_FALSE;
}

static switch_status_t audio_bridge_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CUSTOM ROUTING\n", switch_channel_get_name(channel));

	/* put the channel in a passive state so we can loop audio to it */
	switch_channel_set_state(channel, CS_CONSUME_MEDIA);
	return SWITCH_STATUS_FALSE;
}

static switch_status_t audio_bridge_on_consume_media(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CUSTOM HOLD\n", switch_channel_get_name(channel));

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

static switch_status_t uuid_bridge_on_reset(switch_core_session_t *session);
static switch_status_t uuid_bridge_on_hibernate(switch_core_session_t *session);
static switch_status_t uuid_bridge_on_soft_execute(switch_core_session_t *session);

static const switch_state_handler_table_t uuid_bridge_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ NULL,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ uuid_bridge_on_soft_execute,
	/*.on_consume_media */ uuid_bridge_on_hibernate,
	/*.on_hibernate */ uuid_bridge_on_hibernate,
	/*.on_reset */ uuid_bridge_on_reset
};

static switch_status_t uuid_bridge_on_reset(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CUSTOM RESET\n", switch_channel_get_name(channel));

	switch_channel_clear_flag(channel, CF_ORIGINATING);

	cleanup_proxy_mode_b(session);

	if (switch_channel_test_flag(channel, CF_UUID_BRIDGE_ORIGINATOR)) {
		switch_channel_set_state(channel, CS_SOFT_EXECUTE);
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t uuid_bridge_on_hibernate(switch_core_session_t *session)
{
	switch_channel_set_state(switch_core_session_get_channel(session), CS_RESET);
	return SWITCH_STATUS_FALSE;
}

static switch_status_t uuid_bridge_on_soft_execute(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_core_session_t *other_session = NULL;
	const char *other_uuid = NULL;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s CUSTOM SOFT_EXECUTE\n", switch_channel_get_name(channel));
	switch_channel_clear_state_handler(channel, &uuid_bridge_state_handlers);

	if (!switch_channel_test_flag(channel, CF_UUID_BRIDGE_ORIGINATOR)) {
		return SWITCH_STATUS_SUCCESS;
	}

	if ((other_uuid = switch_channel_get_variable(channel, SWITCH_UUID_BRIDGE)) && (other_session = switch_core_session_locate(other_uuid))) {
		switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
		switch_event_t *event;
		int ready_a, ready_b;
		switch_channel_state_t state, running_state;
		int max = 1000, loops = max;

		switch_channel_set_variable(channel, SWITCH_UUID_BRIDGE, NULL);
		
		for (;;) {
			state = switch_channel_get_state(other_channel);
			running_state = switch_channel_get_running_state(other_channel);

			if (switch_channel_down_nosig(other_channel) || switch_channel_down(channel)) {
				break;
			}

			if (state < CS_HANGUP && state == running_state) {
				
				if (--loops < 1) {
					switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
					switch_channel_hangup(other_channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
				}

				if (running_state == CS_RESET) {
					switch_channel_set_state(other_channel, CS_SOFT_EXECUTE);
				}

				if (running_state == CS_SOFT_EXECUTE) {

					if (switch_channel_test_flag(other_channel, CF_UUID_BRIDGE_ORIGINATOR)) {
						goto done;
					} else {
						break;
					}
				}

			} else {
				loops = max;
			}
			
			switch_yield(20000);
		}

		switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);

		if (switch_ivr_wait_for_answer(session, other_session) != SWITCH_STATUS_SUCCESS) {
			if (switch_true(switch_channel_get_variable(channel, "uuid_bridge_continue_on_cancel"))) {
				switch_channel_set_state(channel, CS_EXECUTE);
			} else if (!switch_channel_test_flag(channel, CF_TRANSFER)) {
				switch_channel_hangup(channel, SWITCH_CAUSE_ORIGINATOR_CANCEL);
			}
			goto done;
		}

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
			goto done;
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
		if (!switch_channel_test_flag(channel, CF_TRANSFER) &&
			!switch_channel_test_flag(channel, CF_REDIRECT) && state < CS_HANGUP && state != CS_ROUTING && state != CS_PARK) {
			switch_channel_set_state(channel, CS_EXECUTE);
		}
	} else {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
	}

 done:

	if (other_session) {
		switch_core_session_rwunlock(other_session);
		other_session = NULL;
	}

	switch_channel_clear_flag(channel, CF_UUID_BRIDGE_ORIGINATOR);

	return SWITCH_STATUS_FALSE;
}

static switch_status_t sb_on_dtmf(switch_core_session_t *session, const switch_dtmf_t *dtmf, switch_dtmf_direction_t direction)
{
	switch_channel_t *channel = NULL;
	char *key;

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	if ((key = (char *) switch_channel_get_private(channel, "__bridge_term_key")) && dtmf->digit == *key) {
		const char *uuid;
		switch_core_session_t *other_session;

		if (switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR)) {
			switch_channel_set_state(channel, CS_EXECUTE);
		} else {
			if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE)) && (other_session = switch_core_session_locate(uuid))) {
				switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
				switch_channel_set_state(other_channel, CS_EXECUTE);
				switch_core_session_rwunlock(other_session);
			} else {
				return SWITCH_STATUS_SUCCESS;
			}
		}

		return SWITCH_STATUS_FALSE;
	}

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t hanguphook(switch_core_session_t *session)
{
	switch_core_session_message_t msg = { 0 };
	switch_channel_t *channel = NULL;
	switch_event_t *event;

	channel = switch_core_session_get_channel(session);


	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	msg.from = __FILE__;
	msg.string_arg = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE);

	
	if (switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR)) {
		switch_channel_clear_flag_recursive(channel, CF_BRIDGE_ORIGINATOR);
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_channel_event_set_data(channel, event);
			switch_event_fire(&event);
		}
	}

	
	switch_core_session_receive_message(session, &msg);
	switch_core_event_hook_remove_state_change(session, hanguphook);

	return SWITCH_STATUS_SUCCESS;

}

static switch_status_t signal_bridge_on_hibernate(switch_core_session_t *session)
{
	switch_channel_t *channel = NULL;
	const char *key;
	switch_core_session_message_t msg = { 0 };
	switch_event_t *event = NULL;
	switch_ivr_dmachine_t *dmachine[2] = { 0 };

	channel = switch_core_session_get_channel(session);
	switch_assert(channel != NULL);

	msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
	msg.from = __FILE__;
	msg.string_arg = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE);

	switch_core_event_hook_add_state_change(session, hanguphook);

	switch_core_session_receive_message(session, &msg);

	if ((key = switch_channel_get_variable(channel, "bridge_terminate_key"))) {
		switch_channel_set_private(channel, "__bridge_term_key", switch_core_session_strdup(session, key));
		switch_core_event_hook_add_recv_dtmf(session, sb_on_dtmf);
	}

	switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE));
	switch_channel_set_variable(channel, SWITCH_LAST_BRIDGE_VARIABLE, switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE));
	
	switch_channel_set_bridge_time(channel);


	if (switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR)) {
		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_core_session_t *other_session;

			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(session));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", msg.string_arg);
			switch_channel_event_set_data(channel, event);
			if ((other_session = switch_core_session_locate(msg.string_arg))) {
				switch_channel_t *other_channel = switch_core_session_get_channel(other_session);

				switch_channel_set_bridge_time(other_channel);

				switch_event_add_presence_data_cols(other_channel, event, "Bridge-B-PD-");
				switch_core_session_rwunlock(other_session);
			}
			switch_event_fire(&event);
		}
	}

	if ((dmachine[0] = switch_core_session_get_dmachine(session, DIGIT_TARGET_SELF)) || 
		(dmachine[1] = switch_core_session_get_dmachine(session, DIGIT_TARGET_PEER))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, 
						  "%s not hibernating due to active digit parser, semi-hibernation engaged.\n", switch_channel_get_name(channel));

		while(switch_channel_ready(channel) && switch_channel_get_state(channel) == CS_HIBERNATE) {
			if (!switch_channel_test_flag(channel, CF_BROADCAST)) {
				if (dmachine[0]) {
					switch_ivr_dmachine_ping(dmachine[0], NULL);
				}
				if (dmachine[1]) {
					switch_ivr_dmachine_ping(dmachine[1], NULL);
				}
			}
			switch_yield(20000);
			switch_ivr_parse_all_messages(session);
		}
	}
	

	return SWITCH_STATUS_SUCCESS;
}

static switch_status_t signal_bridge_on_hangup(switch_core_session_t *session)
{
	const char *uuid;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_core_session_t *other_session;
	switch_event_t *event;

	if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE))) {
		switch_channel_set_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);
	}

	if (switch_channel_get_private(channel, "__bridge_term_key")) {
		switch_core_event_hook_remove_recv_dtmf(session, sb_on_dtmf);
		switch_channel_set_private(channel, "__bridge_term_key", NULL);
	}

	switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, NULL);

	if (uuid && (other_session = switch_core_session_locate(uuid))) {
		switch_channel_t *other_channel = switch_core_session_get_channel(other_session);
		const char *sbv = switch_channel_get_variable(other_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE);
		const char *var;

		if (!zstr(sbv) && !strcmp(sbv, switch_core_session_get_uuid(session))) {

			switch_channel_set_variable(other_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);
			switch_channel_set_variable(other_channel, SWITCH_BRIDGE_VARIABLE, NULL);
			switch_channel_set_variable(other_channel, "call_uuid", switch_core_session_get_uuid(other_session));

			if (switch_channel_up_nosig(other_channel)) {

				if (switch_true(switch_channel_get_variable(other_channel, SWITCH_PARK_AFTER_BRIDGE_VARIABLE))) {
					switch_ivr_park_session(other_session);
				} else if ((var = switch_channel_get_variable(other_channel, SWITCH_TRANSFER_AFTER_BRIDGE_VARIABLE))) {
					transfer_after_bridge(other_session, var);
				}

				if (switch_channel_test_flag(other_channel, CF_BRIDGE_ORIGINATOR)) {
					if (switch_channel_test_flag(channel, CF_ANSWERED) &&
						switch_true(switch_channel_get_variable(other_channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE))) {

						if (switch_channel_test_flag(channel, CF_INTERCEPTED)) {
							switch_channel_set_flag(other_channel, CF_INTERCEPT);
						}
						switch_channel_hangup(other_channel, switch_channel_get_cause(channel));
					} else {
						switch_channel_set_state(other_channel, CS_EXECUTE);
					}
				} else {
					switch_channel_hangup(other_channel, switch_channel_get_cause(channel));
				}
			}
		}
		
		if (switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR)) {
			switch_channel_clear_flag_recursive(channel, CF_BRIDGE_ORIGINATOR);
			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(session));
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", uuid);
				switch_event_add_presence_data_cols(other_channel, event, "Bridge-B-PD-");
				switch_channel_event_set_data(channel, event);
				switch_event_fire(&event);
			}
		}

		switch_core_session_rwunlock(other_session);
	} else {
		if (switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR)) {
			switch_channel_clear_flag_recursive(channel, CF_BRIDGE_ORIGINATOR);
			if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(session));
				switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", uuid);
				switch_channel_event_set_data(channel, event);
				switch_event_fire(&event);
			}
		}
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

static void check_bridge_export(switch_channel_t *channel, switch_channel_t *peer_channel)
{
	switch_caller_profile_t *originator_cp, *originatee_cp;

	originator_cp = switch_channel_get_caller_profile(channel);
	originatee_cp = switch_channel_get_caller_profile(peer_channel);

	originator_cp->callee_id_name = switch_core_strdup(originator_cp->pool, originatee_cp->callee_id_name);
	originator_cp->callee_id_number = switch_core_strdup(originator_cp->pool, originatee_cp->callee_id_number);
	

	switch_channel_process_export(peer_channel, channel, NULL, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
	switch_channel_process_export(channel, peer_channel, NULL, SWITCH_BRIDGE_EXPORT_VARS_VARIABLE);
}

SWITCH_DECLARE(switch_status_t) switch_ivr_signal_bridge(switch_core_session_t *session, switch_core_session_t *peer_session)
{
	switch_channel_t *caller_channel = switch_core_session_get_channel(session);
	switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
	switch_event_t *event;

	if (switch_channel_down_nosig(peer_channel)) {
		switch_channel_hangup(caller_channel, switch_channel_get_cause(peer_channel));
		return SWITCH_STATUS_FALSE;
	}

	if (!switch_channel_up_nosig(caller_channel)) {
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_ORIGINATOR_CANCEL);
		return SWITCH_STATUS_FALSE;
	}

	check_bridge_export(caller_channel, peer_channel);

	switch_channel_set_flag_recursive(caller_channel, CF_SIGNAL_BRIDGE_TTL);
	switch_channel_set_flag_recursive(peer_channel, CF_SIGNAL_BRIDGE_TTL);

	switch_channel_set_variable(caller_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, switch_core_session_get_uuid(peer_session));
	switch_channel_set_variable(peer_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));
	switch_channel_set_variable(peer_channel, "call_uuid", switch_core_session_get_uuid(session));

	switch_channel_set_flag_recursive(caller_channel, CF_BRIDGE_ORIGINATOR);
	switch_channel_clear_flag(peer_channel, CF_BRIDGE_ORIGINATOR);

	switch_channel_clear_state_handler(caller_channel, NULL);
	switch_channel_clear_state_handler(peer_channel, NULL);

	switch_channel_add_state_handler(caller_channel, &signal_bridge_state_handlers);
	switch_channel_add_state_handler(peer_channel, &signal_bridge_state_handlers);

	switch_channel_set_variable(caller_channel, "signal_bridge", "true");
	switch_channel_set_variable(peer_channel, "signal_bridge", "true");

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

	switch_channel_set_state_flag(caller_channel, CF_RESET);
	switch_channel_set_state_flag(peer_channel, CF_RESET);

	switch_channel_set_state(caller_channel, CS_HIBERNATE);
	switch_channel_set_state(peer_channel, CS_HIBERNATE);

#if 0
	if (switch_channel_test_flag(caller_channel, CF_BRIDGED)) {
		switch_channel_set_flag(caller_channel, CF_TRANSFER);
		switch_channel_set_flag(peer_channel, CF_TRANSFER);
	}
#endif

	switch_ivr_bridge_display(session, peer_session);

	return SWITCH_STATUS_SUCCESS;
}

static void abort_call(switch_channel_t *caller_channel, switch_channel_t *peer_channel)
{
	switch_call_cause_t cause = switch_channel_get_cause(caller_channel);
	
	if (!cause) {
		cause = SWITCH_CAUSE_ORIGINATOR_CANCEL;
	}
	
	switch_channel_hangup(peer_channel, cause);
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
	const char *var;
	switch_call_cause_t cause;
	switch_core_session_message_t msg = { 0 };

	if (switch_channel_test_flag(caller_channel, CF_PROXY_MODE)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Call has no media... Redirecting to signal bridge.\n");
		return switch_ivr_signal_bridge(session, peer_session);
	}

	check_bridge_export(caller_channel, peer_channel);
	
	switch_channel_set_flag_recursive(caller_channel, CF_MEDIA_BRIDGE_TTL);
	switch_channel_set_flag_recursive(peer_channel, CF_MEDIA_BRIDGE_TTL);

	switch_channel_set_flag_recursive(caller_channel, CF_BRIDGE_ORIGINATOR);
	switch_channel_clear_flag(peer_channel, CF_BRIDGE_ORIGINATOR);

	switch_channel_audio_sync(caller_channel);
	switch_channel_audio_sync(peer_channel);

	b_leg->session = peer_session;
	switch_copy_string(b_leg->b_uuid, switch_core_session_get_uuid(session), sizeof(b_leg->b_uuid));
	b_leg->stream_id = stream_id;
	b_leg->input_callback = input_callback;
	b_leg->session_data = peer_session_data;
	b_leg->clean_exit = 0;

	a_leg->session = session;
	switch_copy_string(a_leg->b_uuid, switch_core_session_get_uuid(peer_session), sizeof(a_leg->b_uuid));
	a_leg->stream_id = stream_id;
	a_leg->input_callback = input_callback;
	a_leg->session_data = session_data;
	a_leg->clean_exit = 0;

	switch_channel_add_state_handler(peer_channel, &audio_bridge_peer_state_handlers);

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) && !switch_channel_test_flag(caller_channel, CF_ANSWERED)) {
		switch_channel_pass_callee_id(peer_channel, caller_channel);
		switch_channel_answer(caller_channel);
	}

	if (switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA) ||
		switch_channel_test_flag(peer_channel, CF_RING_READY)) {
		const char *app, *data;
		
 		if (!switch_channel_ready(caller_channel)) {
			abort_call(caller_channel, peer_channel);
			goto done;
		}

		switch_channel_set_state(peer_channel, CS_CONSUME_MEDIA);

		switch_channel_set_variable(peer_channel, "call_uuid", switch_core_session_get_uuid(session));
		
		switch_channel_set_bridge_time(caller_channel);
		switch_channel_set_bridge_time(peer_channel);

		if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_BRIDGE) == SWITCH_STATUS_SUCCESS) {
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(session));
			switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", switch_core_session_get_uuid(peer_session));
			switch_channel_event_set_data(caller_channel, event);
			switch_event_add_presence_data_cols(peer_channel, event, "Bridge-B-PD-");
			switch_event_fire(&event);
			br = 1;
		}

		if (switch_core_session_read_lock(peer_session) == SWITCH_STATUS_SUCCESS) {
			switch_channel_set_variable(caller_channel, SWITCH_BRIDGE_VARIABLE, switch_core_session_get_uuid(peer_session));
			switch_channel_set_variable(peer_channel, SWITCH_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));
			switch_channel_set_variable(caller_channel, SWITCH_LAST_BRIDGE_VARIABLE, switch_core_session_get_uuid(peer_session));
			switch_channel_set_variable(peer_channel, SWITCH_LAST_BRIDGE_VARIABLE, switch_core_session_get_uuid(session));

			if (!switch_channel_ready(caller_channel)) {
				abort_call(caller_channel, peer_channel);
				switch_core_session_rwunlock(peer_session);
				goto done;
			}

			if (!switch_channel_media_ready(caller_channel) ||
				(!switch_channel_test_flag(peer_channel, CF_ANSWERED) && !switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA))) {
				if ((status = switch_ivr_wait_for_answer(session, peer_session)) != SWITCH_STATUS_SUCCESS || !switch_channel_ready(caller_channel)) {
					switch_channel_state_t w_state = switch_channel_get_state(caller_channel);
					switch_channel_hangup(peer_channel, SWITCH_CAUSE_ALLOTTED_TIMEOUT);
					if (w_state < CS_HANGUP && w_state != CS_ROUTING && w_state != CS_PARK &&
						!switch_channel_test_flag(caller_channel, CF_REDIRECT) && !switch_channel_test_flag(caller_channel, CF_TRANSFER) &&
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
					abort_call(caller_channel, peer_channel);
					switch_core_session_rwunlock(peer_session);
					goto done;
				}
			}

			if (switch_channel_test_flag(peer_channel, CF_ANSWERED) && !switch_channel_test_flag(caller_channel, CF_ANSWERED)) {
				switch_channel_answer(caller_channel);
			}

			switch_channel_wait_for_flag(peer_channel, CF_BROADCAST, SWITCH_FALSE, 10000, caller_channel);
			switch_ivr_parse_all_events(peer_session);
			switch_ivr_parse_all_events(session);

			msg.message_id = SWITCH_MESSAGE_INDICATE_BRIDGE;
			msg.from = __FILE__;
			msg.string_arg = switch_core_session_strdup(peer_session, switch_core_session_get_uuid(session));

			if (switch_core_session_receive_message(peer_session, &msg) != SWITCH_STATUS_SUCCESS) {
				status = SWITCH_STATUS_FALSE;
				abort_call(caller_channel, peer_channel);
				switch_core_session_rwunlock(peer_session);
				goto done;
			}

			msg.string_arg = switch_core_session_strdup(session, switch_core_session_get_uuid(peer_session));
			if (switch_core_session_receive_message(session, &msg) != SWITCH_STATUS_SUCCESS) {
				status = SWITCH_STATUS_FALSE;
				abort_call(caller_channel, peer_channel);
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
				switch_channel_set_variable(caller_channel, "bridge_pre_execute_app", app);
				
				if ((data = switch_channel_get_variable(caller_channel, "bridge_pre_execute_aleg_data"))) {
					switch_channel_set_variable(caller_channel, "bridge_pre_execute_data", data);
				}
			}

			if ((app = switch_channel_get_variable(caller_channel, "bridge_pre_execute_bleg_app"))) {
				switch_channel_set_variable(peer_channel, "bridge_pre_execute_app", app);

				if ((data = switch_channel_get_variable(caller_channel, "bridge_pre_execute_bleg_data"))) {
					switch_channel_set_variable(peer_channel, "bridge_pre_execute_data", data);
				}
				
			}
			
			switch_channel_set_private(peer_channel, "_bridge_", b_leg);
			switch_channel_set_state(peer_channel, CS_EXCHANGE_MEDIA);

			audio_bridge_thread(NULL, (void *) a_leg);

			switch_channel_clear_flag_recursive(caller_channel, CF_BRIDGE_ORIGINATOR);

			switch_channel_stop_broadcast(peer_channel);


			while (switch_channel_get_state(peer_channel) == CS_EXCHANGE_MEDIA) {
				switch_ivr_parse_all_messages(session);
				switch_cond_next();
			}

			if (inner_bridge) {
				if (switch_channel_ready(caller_channel)) {
					switch_channel_set_flag(caller_channel, CF_BRIDGED);
				}

				if (switch_channel_ready(peer_channel)) {
					switch_channel_set_flag(peer_channel, CF_BRIDGED);
				}
			}

			if ((cause = switch_channel_get_cause(caller_channel))) {
				switch_channel_set_variable(peer_channel, SWITCH_BRIDGE_HANGUP_CAUSE_VARIABLE, switch_channel_cause2str(cause));
			}

			if ((cause = switch_channel_get_cause(peer_channel))) {
				switch_channel_set_variable(caller_channel, SWITCH_BRIDGE_HANGUP_CAUSE_VARIABLE, switch_channel_cause2str(cause));
			}
			
			if (switch_channel_down_nosig(peer_channel)) {
				switch_bool_t copy_xml_cdr = switch_true(switch_channel_get_variable(peer_channel, SWITCH_COPY_XML_CDR_VARIABLE));
				switch_bool_t copy_json_cdr = switch_true(switch_channel_get_variable(peer_channel, SWITCH_COPY_JSON_CDR_VARIABLE));

				if (copy_xml_cdr || copy_json_cdr) {
					char *cdr_text = NULL;					

					switch_channel_wait_for_state(peer_channel, caller_channel, CS_DESTROY);

					if (copy_xml_cdr) {
						switch_xml_t cdr = NULL;

						if (switch_ivr_generate_xml_cdr(peer_session, &cdr) == SWITCH_STATUS_SUCCESS) {
							cdr_text = switch_xml_toxml(cdr, SWITCH_FALSE);
							switch_xml_free(cdr);
						}
					}
					if (copy_json_cdr) {
						cJSON *cdr = NULL;

						if (switch_ivr_generate_json_cdr(peer_session, &cdr, SWITCH_TRUE) == SWITCH_STATUS_SUCCESS) {
							cdr_text = cJSON_PrintUnformatted(cdr);
							cJSON_Delete(cdr);
						}
					}

					if (cdr_text) {
						switch_channel_set_variable(caller_channel, "b_leg_cdr", cdr_text);
						switch_channel_set_variable_name_printf(caller_channel, cdr_text, "b_leg_cdr_%s", switch_core_session_get_uuid(peer_session));
						switch_safe_free(cdr_text);
					}
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
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Bridge Failed %s->%s\n",
						  switch_channel_get_name(caller_channel), switch_channel_get_name(peer_channel)
			);
		switch_channel_hangup(peer_channel, SWITCH_CAUSE_NO_ANSWER);
	}

  done:

	switch_channel_set_variable(peer_channel, "call_uuid", switch_core_session_get_uuid(peer_session));

	if (br && switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNBRIDGE) == SWITCH_STATUS_SUCCESS) {
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-A-Unique-ID", switch_core_session_get_uuid(session));
		switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Bridge-B-Unique-ID", switch_core_session_get_uuid(peer_session));
		switch_channel_event_set_data(caller_channel, event);
		switch_event_add_presence_data_cols(peer_channel, event, "Bridge-B-PD-");
		switch_event_fire(&event);
	}

	msg.message_id = SWITCH_MESSAGE_INDICATE_UNBRIDGE;
	msg.from = __FILE__;
	msg.string_arg = switch_core_session_strdup(peer_session, switch_core_session_get_uuid(session));
	switch_core_session_receive_message(peer_session, &msg);

	msg.string_arg = switch_core_session_strdup(session, switch_core_session_get_uuid(peer_session));
	switch_core_session_receive_message(session, &msg);

	state = switch_channel_get_state(caller_channel);


	if (!switch_channel_test_flag(caller_channel, CF_TRANSFER) && !switch_channel_test_flag(caller_channel, CF_REDIRECT) &&
		!switch_channel_test_flag(caller_channel, CF_XFER_ZOMBIE) && !a_leg->clean_exit && !inner_bridge) {
		if ((state != CS_EXECUTE && state != CS_SOFT_EXECUTE && state != CS_PARK && state != CS_ROUTING) ||
			(switch_channel_test_flag(peer_channel, CF_ANSWERED) && state < CS_HANGUP)) {
			switch_call_cause_t cause = switch_channel_get_cause(peer_channel);

			if (cause && !switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
				switch_channel_handle_cause(caller_channel, cause);
			}

			if (!switch_channel_test_flag(caller_channel, CF_TRANSFER)) {
				if (switch_true(switch_channel_get_variable(caller_channel, SWITCH_PARK_AFTER_BRIDGE_VARIABLE))) {
					switch_ivr_park_session(session);
				} else if ((var = switch_channel_get_variable(caller_channel, SWITCH_TRANSFER_AFTER_BRIDGE_VARIABLE))) {
					transfer_after_bridge(session, var);
				} else {
					const char *hup = switch_channel_get_variable(caller_channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE);
					int explicit = 0;
					
					if (hup) {
						explicit = !strcasecmp(hup, "explicit");
					}
					
					if (explicit || (switch_channel_test_flag(peer_channel, CF_ANSWERED) && switch_true(hup))) {
						switch_call_cause_t cause = switch_channel_get_cause(peer_channel);
						if (cause == SWITCH_CAUSE_NONE) {
							cause = SWITCH_CAUSE_NORMAL_CLEARING;
						}
						
						if (switch_channel_test_flag(peer_channel, CF_INTERCEPTED)) {
							switch_channel_set_flag(peer_channel, CF_INTERCEPT);
						}
						switch_channel_hangup(caller_channel, cause);
					}
				}
			}
		}
	}

	if (switch_channel_test_flag(caller_channel, CF_REDIRECT)) {
		if (switch_channel_test_flag(caller_channel, CF_RESET)) {
			switch_channel_clear_flag(caller_channel, CF_RESET);
		} else {
			state = switch_channel_get_state(caller_channel);
			if (!(state == CS_RESET || state == CS_PARK || state == CS_ROUTING)) {
				switch_channel_set_state(caller_channel, CS_RESET);
			}
		}
	}

	return status;
}

static void cleanup_proxy_mode_b(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);


	if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
		switch_ivr_media(switch_core_session_get_uuid(session), SMF_NONE);
	}
}


static void cleanup_proxy_mode_a(switch_core_session_t *session)
{
	switch_core_session_t *sbsession;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	int done = 0;

	if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
		if (switch_core_session_get_partner(session, &sbsession) == SWITCH_STATUS_SUCCESS) {
			switch_channel_t *sbchannel = switch_core_session_get_channel(sbsession);

			if (switch_channel_test_flag(sbchannel, CF_PROXY_MODE)) { 	
				/* Clear this now, otherwise will cause the one we're interested in to hang up too...*/
				switch_channel_set_variable(sbchannel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);
				switch_channel_hangup(sbchannel, SWITCH_CAUSE_ATTENDED_TRANSFER);
			} else {
				done = 1;
			}
			switch_core_session_rwunlock(sbsession);
		}
	}

	if (done) return;
	
	switch_channel_set_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);
	switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, NULL);
	switch_channel_set_variable(channel, SWITCH_BRIDGE_UUID_VARIABLE, NULL);

}


SWITCH_DECLARE(switch_status_t) switch_ivr_uuid_bridge(const char *originator_uuid, const char *originatee_uuid)
{
	switch_core_session_t *originator_session, *originatee_session, *swap_session;
	switch_channel_t *originator_channel, *originatee_channel, *swap_channel;
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_caller_profile_t *originator_cp, *originatee_cp;
	switch_channel_state_t state;

	if ((originator_session = switch_core_session_locate(originator_uuid))) {
		if ((originatee_session = switch_core_session_locate(originatee_uuid))) {
			originator_channel = switch_core_session_get_channel(originator_session);
			originatee_channel = switch_core_session_get_channel(originatee_session);


			if (switch_channel_test_flag(originator_channel, CF_LEG_HOLDING)) {
				switch_channel_set_flag(originator_channel, CF_HOLD_ON_BRIDGE);
			}

			if (switch_channel_test_flag(originatee_channel, CF_LEG_HOLDING)) {
				switch_channel_set_flag(originatee_channel, CF_HOLD_ON_BRIDGE);
			}


			if (switch_channel_direction(originator_channel) == SWITCH_CALL_DIRECTION_OUTBOUND && !switch_channel_test_flag(originator_channel, CF_DIALPLAN)) {
				switch_channel_flip_cid(originator_channel);
				switch_channel_set_flag(originator_channel, CF_DIALPLAN);
			}

			if (switch_channel_down_nosig(originator_channel)) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(originator_session), SWITCH_LOG_DEBUG, "%s is hungup refusing to bridge.\n", switch_channel_get_name(originatee_channel));
				switch_core_session_rwunlock(originator_session);
				switch_core_session_rwunlock(originatee_session);
				return SWITCH_STATUS_FALSE;
			}

			if (!switch_channel_media_up(originator_channel)) {
				if (switch_channel_media_up(originatee_channel)) {
					swap_session = originator_session;
					originator_session = originatee_session;
					originatee_session = swap_session;

					swap_channel = originator_channel;
					originator_channel = originatee_channel;
					originatee_channel = swap_channel;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(originatee_session), SWITCH_LOG_WARNING, "reversing order of channels so this will work!\n");
				} else {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(originator_session), SWITCH_LOG_CRIT, "Neither channel is answered, cannot bridge them.\n");
					switch_core_session_rwunlock(originator_session);
					switch_core_session_rwunlock(originatee_session);
					return SWITCH_STATUS_FALSE;
				}
			}

			cleanup_proxy_mode_a(originator_session);
			cleanup_proxy_mode_a(originatee_session);

			/* override transmit state for originator_channel to bridge to originatee_channel 
			 * install pointer to originatee_session into originator_channel
			 * set CF_TRANSFER on both channels and change state to CS_SOFT_EXECUTE to
			 * interrupt anything they are already doing.
			 * originatee_session will fall asleep and originator_session will bridge to it
			 */

			switch_channel_set_flag(originator_channel, CF_REDIRECT);
			switch_channel_set_flag(originatee_channel, CF_REDIRECT);


			switch_channel_set_variable(originator_channel, SWITCH_UUID_BRIDGE, switch_core_session_get_uuid(originatee_session));
			switch_channel_set_variable(originator_channel, SWITCH_BRIDGE_CHANNEL_VARIABLE, switch_channel_get_name(originatee_channel));
			switch_channel_set_variable(originator_channel, SWITCH_BRIDGE_UUID_VARIABLE, switch_core_session_get_uuid(originatee_session));
			switch_channel_set_variable(originator_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(originatee_session));
			switch_channel_set_variable(originatee_channel, SWITCH_BRIDGE_CHANNEL_VARIABLE, switch_channel_get_name(originator_channel));
			switch_channel_set_variable(originatee_channel, SWITCH_BRIDGE_UUID_VARIABLE, switch_core_session_get_uuid(originator_session));
			switch_channel_set_variable(originatee_channel, SWITCH_SIGNAL_BOND_VARIABLE, switch_core_session_get_uuid(originator_session));


			originator_cp = switch_channel_get_caller_profile(originator_channel);
			originatee_cp = switch_channel_get_caller_profile(originatee_channel);


			
			if (switch_channel_outbound_display(originator_channel)) {
				switch_channel_invert_cid(originator_channel);
				
				if (switch_channel_direction(originator_channel) == SWITCH_CALL_DIRECTION_INBOUND) {
					switch_channel_clear_flag(originatee_channel, CF_BLEG);
				}
			}

			if (switch_channel_inbound_display(originatee_channel)) {
				switch_channel_invert_cid(originatee_channel);
				
				if (switch_channel_direction(originatee_channel) == SWITCH_CALL_DIRECTION_INBOUND) {
					switch_channel_set_flag(originatee_channel, CF_BLEG);
				}

			}


			switch_channel_set_variable(originatee_channel, "original_destination_number", originatee_cp->destination_number);
			switch_channel_set_variable(originatee_channel, "original_caller_id_name", originatee_cp->caller_id_name);
			switch_channel_set_variable(originatee_channel, "original_caller_id_number", originatee_cp->caller_id_number);

			switch_channel_set_variable(originator_channel, "original_destination_number", originator_cp->destination_number);
			switch_channel_set_variable(originator_channel, "original_caller_id_name", originator_cp->caller_id_name);
			switch_channel_set_variable(originator_channel, "original_caller_id_number", originator_cp->caller_id_number);

			switch_channel_step_caller_profile(originatee_channel);
			switch_channel_step_caller_profile(originator_channel);

			originator_cp = switch_channel_get_caller_profile(originator_channel);
			originatee_cp = switch_channel_get_caller_profile(originatee_channel);


#ifdef DEEP_DEBUG_CID
			{
				switch_event_t *event;

				if (switch_event_create_plain(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
					//switch_channel_event_set_basic_data(originator_channel, event);
					switch_caller_profile_event_set_data(originator_cp, "ORIGINATOR", event);
					switch_caller_profile_event_set_data(originatee_cp, "ORIGINATEE", event);
					DUMP_EVENT(event);
					switch_event_destroy(&event);
				}
			}
#endif

			switch_channel_set_originator_caller_profile(originatee_channel, switch_caller_profile_clone(originatee_session, originator_cp));
			switch_channel_set_originatee_caller_profile(originator_channel, switch_caller_profile_clone(originator_session, originatee_cp));
			
			originator_cp->callee_id_name = switch_core_strdup(originator_cp->pool, originatee_cp->callee_id_name);
			originator_cp->callee_id_number = switch_core_strdup(originator_cp->pool, originatee_cp->callee_id_number);

			originatee_cp->caller_id_name = switch_core_strdup(originatee_cp->pool, originator_cp->caller_id_name);
			originatee_cp->caller_id_number = switch_core_strdup(originatee_cp->pool, originator_cp->caller_id_number);

#ifdef DEEP_DEBUG_CID
			{
				switch_event_t *event;

				if (switch_event_create_plain(&event, SWITCH_EVENT_CHANNEL_DATA) == SWITCH_STATUS_SUCCESS) {
					//switch_channel_event_set_basic_data(originator_channel, event);
					switch_caller_profile_event_set_data(originator_cp, "POST-ORIGINATOR", event);
					switch_caller_profile_event_set_data(originatee_cp, "POST-ORIGINATEE", event);
					DUMP_EVENT(event);
					switch_event_destroy(&event);
				}
			}
#endif

			switch_channel_stop_broadcast(originator_channel);
			switch_channel_stop_broadcast(originatee_channel);

			switch_channel_set_flag(originator_channel, CF_TRANSFER);
			switch_channel_set_flag(originatee_channel, CF_TRANSFER);


			switch_channel_clear_flag(originator_channel, CF_ORIGINATING);
			switch_channel_clear_flag(originatee_channel, CF_ORIGINATING);


			originator_cp->transfer_source = switch_core_sprintf(originator_cp->pool, 
																 "%ld:%s:uuid_br:%s", (long)switch_epoch_time_now(NULL), originator_cp->uuid_str, 
																 switch_core_session_get_uuid(originatee_session));
			switch_channel_add_variable_var_check(originator_channel, SWITCH_TRANSFER_HISTORY_VARIABLE, 
												  originator_cp->transfer_source, SWITCH_FALSE, SWITCH_STACK_PUSH);
			switch_channel_set_variable(originator_channel, SWITCH_TRANSFER_SOURCE_VARIABLE, originator_cp->transfer_source);

			
			originatee_cp->transfer_source = switch_core_sprintf(originatee_cp->pool,
																 "%ld:%s:uuid_br:%s", (long)switch_epoch_time_now(NULL), originatee_cp->uuid_str, 
																 switch_core_session_get_uuid(originator_session));
			switch_channel_add_variable_var_check(originatee_channel, SWITCH_TRANSFER_HISTORY_VARIABLE, 
												  originatee_cp->transfer_source, SWITCH_FALSE, SWITCH_STACK_PUSH);
			switch_channel_set_variable(originatee_channel, SWITCH_TRANSFER_SOURCE_VARIABLE, originatee_cp->transfer_source);

			/* change the states and let the chips fall where they may */

			//switch_channel_set_variable(originator_channel, SWITCH_PARK_AFTER_BRIDGE_VARIABLE, NULL);
			//switch_channel_set_variable(originatee_channel, SWITCH_PARK_AFTER_BRIDGE_VARIABLE, NULL);
			switch_channel_clear_state_handler(originator_channel, NULL);
			switch_channel_clear_state_handler(originatee_channel, NULL);



			switch_channel_clear_state_flag(originator_channel, CF_BRIDGE_ORIGINATOR);
			switch_channel_clear_state_flag(originatee_channel, CF_BRIDGE_ORIGINATOR);

			switch_channel_clear_flag(originator_channel, CF_UUID_BRIDGE_ORIGINATOR);
			switch_channel_clear_flag(originatee_channel, CF_UUID_BRIDGE_ORIGINATOR);
			switch_channel_set_state_flag(originator_channel, CF_UUID_BRIDGE_ORIGINATOR);

			switch_channel_add_state_handler(originator_channel, &uuid_bridge_state_handlers);
			switch_channel_add_state_handler(originatee_channel, &uuid_bridge_state_handlers);

			state = switch_channel_get_state(originator_channel);
			switch_channel_set_state(originator_channel, state == CS_HIBERNATE ? CS_CONSUME_MEDIA : CS_HIBERNATE);
			state = switch_channel_get_state(originatee_channel);
			switch_channel_set_state(originatee_channel, state == CS_HIBERNATE ? CS_CONSUME_MEDIA : CS_HIBERNATE);

			status = SWITCH_STATUS_SUCCESS;

			//switch_ivr_bridge_display(originator_session, originatee_session);

			/* release the read locks we have on the channels */
			switch_core_session_rwunlock(originator_session);
			switch_core_session_rwunlock(originatee_session);

		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(originator_session), SWITCH_LOG_DEBUG, "originatee uuid %s is not present\n", originatee_uuid);
			switch_core_session_rwunlock(originator_session);
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(originator_session), SWITCH_LOG_DEBUG, "originator uuid %s is not present\n", originator_uuid);
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

		if ((brto = switch_channel_get_variable(rchannel, SWITCH_ORIGINATE_SIGNAL_BOND_VARIABLE)) || 
			(brto = switch_channel_get_partner_uuid(rchannel))) {
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
	const char *buuid, *var;
	char brto[SWITCH_UUID_FORMATTED_LENGTH + 1] = "";
	
	if (bleg) {
		if (switch_ivr_find_bridged_uuid(uuid, brto, sizeof(brto)) == SWITCH_STATUS_SUCCESS) {
			uuid = switch_core_session_strdup(session, brto);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "no uuid bridged to %s\n", uuid);
			return;
		}
	}

	if (zstr(uuid) || !(rsession = switch_core_session_locate(uuid))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "no uuid %s\n", uuid);
		return;
	}

	channel = switch_core_session_get_channel(session);
	rchannel = switch_core_session_get_channel(rsession);
	buuid = switch_channel_get_partner_uuid(rchannel);

	if ((var = switch_channel_get_variable(channel, "intercept_unbridged_only")) && switch_true(var)) {
		if ((switch_channel_test_flag(rchannel, CF_BRIDGED))) {
			switch_core_session_rwunlock(rsession);
			return;
		}
	}

	if ((var = switch_channel_get_variable(channel, "intercept_unanswered_only")) && switch_true(var)) {
		if ((switch_channel_test_flag(rchannel, CF_ANSWERED))) {
			switch_core_session_rwunlock(rsession);
			return;
		}
	}

	switch_channel_answer(channel);

	if (!zstr(buuid)) {
		if ((bsession = switch_core_session_locate(buuid))) {
			bchannel = switch_core_session_get_channel(bsession);
			switch_channel_set_flag(bchannel, CF_INTERCEPT);
		}
	}

	if (!switch_channel_test_flag(rchannel, CF_ANSWERED)) {
		switch_channel_answer(rchannel);
	}

	switch_channel_mark_hold(rchannel, SWITCH_FALSE);

	switch_channel_set_state_flag(rchannel, CF_TRANSFER);
	switch_channel_set_state(rchannel, CS_PARK);

	if (bchannel) {
		switch_channel_set_state_flag(bchannel, CF_TRANSFER);
		switch_channel_set_state(bchannel, CS_PARK);
	}

	switch_channel_set_flag(rchannel, CF_INTERCEPTED);
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
