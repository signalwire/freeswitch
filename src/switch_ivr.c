/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2010, Anthony Minessale II <anthm@freeswitch.org>
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
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Neal Horman <neal at wanlink dot com>
 * Matt Klein <mklein@nmedia.net>
 * Michael Jerris <mike@jerris.com>
 * Ken Rice <krice at suspicious dot org>
 *
 * switch_ivr.c -- IVR Library
 *
 */

#include <switch.h>
#include <switch_ivr.h>
#include "stfu.h"

SWITCH_DECLARE(switch_status_t) switch_ivr_sound_test(switch_core_session_t *session)
{

	switch_codec_implementation_t imp = { 0 };
	switch_codec_t codec = { 0 };
	int16_t peak = 0;
	int16_t *data;
	switch_frame_t *read_frame = NULL;
	uint32_t i;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int64_t global_total = 0, global_sum = 0, period_sum = 0;
	int period_total = 0;
	int period_avg = 0, global_avg = 0;
	int avg = 0;
	int period_len;

	switch_core_session_get_read_impl(session, &imp);

	period_len = imp.actual_samples_per_second / imp.samples_per_packet;

	if (switch_core_codec_init(&codec,
							   "L16",
							   NULL,
							   imp.samples_per_second,
							   imp.microseconds_per_packet / 1000,
							   imp.number_of_channels,
							   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
							   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Error L16@%uhz %u channels %dms\n",
						  imp.samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);
		return SWITCH_STATUS_FALSE;
	}

	while (switch_channel_ready(channel)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (switch_test_flag(read_frame, SFF_CNG) || !read_frame->samples) {
			continue;
		}


		data = (int16_t *) read_frame->data;
		peak = 0;
		avg = 0;
		for (i = 0; i < read_frame->samples; i++) {
			const int16_t s = (int16_t) abs(data[i]);
			if (s > peak) {
				peak = s;
			}
			avg += s;
		}

		avg /= read_frame->samples;

		period_sum += peak;
		global_sum += peak;

		global_total++;
		period_total++;

		period_avg = (int) (period_sum / period_total);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CONSOLE,
						  "\npacket_avg=%d packet_peak=%d period_avg=%d global_avg=%d\n\n", avg, peak, period_avg, global_avg);

		if (period_total >= period_len) {
			global_avg = (int) (global_sum / global_total);
			period_total = 0;
			period_sum = 0;
		}

	}


	switch_core_codec_destroy(&codec);

	return SWITCH_STATUS_SUCCESS;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_sleep(switch_core_session_t *session, uint32_t ms, switch_bool_t sync, switch_input_args_t *args)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_time_t start = switch_micro_time_now(), now, done = switch_micro_time_now() + (ms * 1000);
	switch_frame_t *read_frame, cng_frame = { 0 };
	int32_t left, elapsed;
	char data[2] = "";

	switch_frame_t write_frame = { 0 };
	unsigned char *abuf = NULL;
	switch_codec_implementation_t imp = { 0 };
	switch_codec_t codec = { 0 };
	int sval = 0;
	const char *var;

	/*
	   if (!switch_channel_test_flag(channel, CF_OUTBOUND) && !switch_channel_test_flag(channel, CF_PROXY_MODE) && 
	   !switch_channel_media_ready(channel) && !switch_channel_test_flag(channel, CF_SERVICE)) {
	   if ((status = switch_channel_pre_answer(channel)) != SWITCH_STATUS_SUCCESS) {
	   switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot establish media.\n");
	   return SWITCH_STATUS_FALSE;
	   }
	   }
	 */

	if (!switch_channel_media_ready(channel)) {
		switch_yield(ms * 1000);
		return SWITCH_STATUS_SUCCESS;
	}

	if (ms > 100 && (var = switch_channel_get_variable(channel, SWITCH_SEND_SILENCE_WHEN_IDLE_VARIABLE)) && (sval = atoi(var))) {
		switch_core_session_get_read_impl(session, &imp);

		if (switch_core_codec_init(&codec,
								   "L16",
								   NULL,
								   imp.samples_per_second,
								   imp.microseconds_per_packet / 1000,
								   imp.number_of_channels,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
								   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Error L16@%uhz %u channels %dms\n",
							  imp.samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);
			return SWITCH_STATUS_FALSE;
		}


		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Codec Activated L16@%uhz %u channels %dms\n",
						  imp.samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);

		write_frame.codec = &codec;
		switch_zmalloc(abuf, SWITCH_RECOMMENDED_BUFFER_SIZE);
		write_frame.data = abuf;
		write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
		write_frame.datalen = imp.decoded_bytes_per_packet;
		write_frame.samples = write_frame.datalen / sizeof(int16_t);

	}

	if (!write_frame.datalen) {
		sval = 0;
	}

	cng_frame.data = data;
	cng_frame.datalen = 2;
	cng_frame.buflen = 2;
	switch_set_flag((&cng_frame), SFF_CNG);

	if (sync) {
		switch_channel_audio_sync(channel);
	}

	if (!ms) {
		return SWITCH_STATUS_SUCCESS;
	}

	for (;;) {
		now = switch_micro_time_now();
		elapsed = (int32_t) ((now - start) / 1000);
		left = ms - elapsed;

		if (!switch_channel_ready(channel)) {
			status = SWITCH_STATUS_FALSE;
			break;
		}

		if (switch_channel_test_flag(channel, CF_BREAK)) {
			switch_channel_clear_flag(channel, CF_BREAK);
			status = SWITCH_STATUS_BREAK;
			break;
		}

		if (now > done || left <= 0) {
			break;
		}


		switch_ivr_parse_all_events(session);


		if (args && (args->input_callback || args->buf || args->buflen)) {
			switch_dtmf_t dtmf;

			/*
			   dtmf handler function you can hook up to be executed when a digit is dialed during playback 
			   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
			 */
			if (switch_channel_has_dtmf(channel)) {
				if (!args->input_callback && !args->buf) {
					status = SWITCH_STATUS_BREAK;
					break;
				}
				switch_channel_dequeue_dtmf(channel, &dtmf);
				if (args->input_callback) {
					status = args->input_callback(session, (void *) &dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen);
				} else {
					switch_copy_string((char *) args->buf, (void *) &dtmf, args->buflen);
					status = SWITCH_STATUS_BREAK;
				}
			}

			if (args->input_callback) {
				switch_event_t *event = NULL;

				if (switch_core_session_dequeue_event(session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
					status = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);
					switch_event_destroy(&event);
				}
			}

			if (status != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}

		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (sval && write_frame.datalen) {
			switch_generate_sln_silence((int16_t *) write_frame.data, write_frame.samples, sval);
			switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
		} else {
			switch_core_session_write_frame(session, &cng_frame, SWITCH_IO_FLAG_NONE, 0);
		}
	}

	if (write_frame.codec) {
		switch_core_codec_destroy(&codec);
	}

	switch_safe_free(abuf);

	return status;
}

static void *SWITCH_THREAD_FUNC unicast_thread_run(switch_thread_t *thread, void *obj)
{
	switch_unicast_conninfo_t *conninfo = (switch_unicast_conninfo_t *) obj;
	switch_size_t len;
	switch_channel_t *channel;

	if (!conninfo) {
		return NULL;
	}

	channel = switch_core_session_get_channel(conninfo->session);

	while (switch_test_flag(conninfo, SUF_READY) && switch_test_flag(conninfo, SUF_THREAD_RUNNING)) {
		len = conninfo->write_frame.buflen;
		if (switch_socket_recv(conninfo->socket, conninfo->write_frame.data, &len) != SWITCH_STATUS_SUCCESS || len == 0) {
			break;
		}
		conninfo->write_frame.datalen = (uint32_t) len;
		conninfo->write_frame.samples = conninfo->write_frame.datalen / 2;
		switch_core_session_write_frame(conninfo->session, &conninfo->write_frame, SWITCH_IO_FLAG_NONE, conninfo->stream_id);
	}

	switch_clear_flag_locked(conninfo, SUF_READY);
	switch_clear_flag_locked(conninfo, SUF_THREAD_RUNNING);

	return NULL;
}

static void unicast_thread_launch(switch_unicast_conninfo_t *conninfo)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, switch_core_session_get_pool(conninfo->session));
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_set_flag_locked(conninfo, SUF_THREAD_RUNNING);
	switch_thread_create(&thread, thd_attr, unicast_thread_run, conninfo, switch_core_session_get_pool(conninfo->session));
}

SWITCH_DECLARE(switch_status_t) switch_ivr_deactivate_unicast(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_unicast_conninfo_t *conninfo;
	int sanity = 0;

	if (!switch_channel_test_flag(channel, CF_UNICAST)) {
		return SWITCH_STATUS_FALSE;
	}

	if ((conninfo = switch_channel_get_private(channel, "unicast"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Shutting down unicast connection\n");
		switch_clear_flag_locked(conninfo, SUF_READY);
		switch_socket_shutdown(conninfo->socket, SWITCH_SHUTDOWN_READWRITE);
		while (switch_test_flag(conninfo, SUF_THREAD_RUNNING)) {
			switch_yield(10000);
			if (++sanity >= 10000) {
				break;
			}
		}
		if (switch_core_codec_ready(&conninfo->read_codec)) {
			switch_core_codec_destroy(&conninfo->read_codec);
		}
		switch_socket_close(conninfo->socket);
	}
	switch_channel_clear_flag(channel, CF_UNICAST);
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_activate_unicast(switch_core_session_t *session,
															char *local_ip,
															switch_port_t local_port,
															char *remote_ip, switch_port_t remote_port, char *transport, char *flags)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_unicast_conninfo_t *conninfo = switch_core_session_alloc(session, sizeof(*conninfo));
	switch_codec_t *read_codec;

	switch_assert(conninfo != NULL);

	conninfo->local_ip = switch_core_session_strdup(session, local_ip);
	conninfo->local_port = local_port;

	conninfo->remote_ip = switch_core_session_strdup(session, remote_ip);
	conninfo->remote_port = remote_port;
	conninfo->session = session;

	if (!strcasecmp(transport, "udp")) {
		conninfo->type = AF_INET;
		conninfo->transport = SOCK_DGRAM;
	} else if (!strcasecmp(transport, "tcp")) {
		conninfo->type = AF_INET;
		conninfo->transport = SOCK_STREAM;
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid transport %s\n", transport);
		goto fail;
	}

	if (flags) {
		if (strstr(flags, "native")) {
			switch_set_flag(conninfo, SUF_NATIVE);
		}
	}

	switch_mutex_init(&conninfo->flag_mutex, SWITCH_MUTEX_NESTED, switch_core_session_get_pool(session));

	read_codec = switch_core_session_get_read_codec(session);

	if (!switch_test_flag(conninfo, SUF_NATIVE)) {
		if (switch_core_codec_init(&conninfo->read_codec,
								   "L16",
								   NULL,
								   read_codec->implementation->actual_samples_per_second,
								   read_codec->implementation->microseconds_per_packet / 1000,
								   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE,
								   NULL, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG,
							  "Raw Codec Activation Success L16@%uhz 1 channel %dms\n",
							  read_codec->implementation->actual_samples_per_second, read_codec->implementation->microseconds_per_packet / 1000);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Raw Codec Activation Failed L16@%uhz 1 channel %dms\n",
							  read_codec->implementation->actual_samples_per_second, read_codec->implementation->microseconds_per_packet / 1000);
			goto fail;
		}
	}

	conninfo->write_frame.data = conninfo->write_frame_data;
	conninfo->write_frame.buflen = sizeof(conninfo->write_frame_data);
	conninfo->write_frame.codec = switch_test_flag(conninfo, SUF_NATIVE) ? read_codec : &conninfo->read_codec;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "connect %s:%d->%s:%d\n",
					  conninfo->local_ip, conninfo->local_port, conninfo->remote_ip, conninfo->remote_port);

	if (switch_sockaddr_info_get(&conninfo->local_addr,
								 conninfo->local_ip, SWITCH_UNSPEC, conninfo->local_port, 0,
								 switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		goto fail;
	}

	if (switch_sockaddr_info_get(&conninfo->remote_addr,
								 conninfo->remote_ip, SWITCH_UNSPEC, conninfo->remote_port, 0,
								 switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
		goto fail;
	}

	if (switch_socket_create(&conninfo->socket, AF_INET, SOCK_DGRAM, 0, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
		if (switch_socket_bind(conninfo->socket, conninfo->local_addr) != SWITCH_STATUS_SUCCESS) {
			goto fail;
		}
	} else {
		goto fail;
	}

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "Created unicast connection %s:%d->%s:%d\n",
					  conninfo->local_ip, conninfo->local_port, conninfo->remote_ip, conninfo->remote_port);
	switch_channel_set_private(channel, "unicast", conninfo);
	switch_channel_set_flag(channel, CF_UNICAST);
	switch_set_flag_locked(conninfo, SUF_READY);
	return SWITCH_STATUS_SUCCESS;

  fail:

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_CRIT, "Failure creating unicast connection %s:%d->%s:%d\n",
					  conninfo->local_ip, conninfo->local_port, conninfo->remote_ip, conninfo->remote_port);
	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_parse_event(switch_core_session_t *session, switch_event_t *event)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	char *cmd = switch_event_get_header(event, "call-command");
	unsigned long cmd_hash;
	switch_ssize_t hlen = -1;
	unsigned long CMD_EXECUTE = switch_hashfunc_default("execute", &hlen);
	unsigned long CMD_HANGUP = switch_hashfunc_default("hangup", &hlen);
	unsigned long CMD_NOMEDIA = switch_hashfunc_default("nomedia", &hlen);
	unsigned long CMD_UNICAST = switch_hashfunc_default("unicast", &hlen);
	char *lead_frames = switch_event_get_header(event, "lead-frames");
	char *event_lock = switch_event_get_header(event, "event-lock");
	char *event_lock_pri = switch_event_get_header(event, "event-lock-pri");
	switch_status_t status = SWITCH_STATUS_FALSE;
	int el = 0, elp = 0;

	if (zstr(cmd)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid Command!\n");
		return SWITCH_STATUS_FALSE;
	}

	cmd_hash = switch_hashfunc_default(cmd, &hlen);

	switch_channel_set_flag_recursive(channel, CF_EVENT_PARSE);

	if (switch_true(event_lock)) {
		switch_channel_set_flag_recursive(channel, CF_EVENT_LOCK);
		el = 1;
	}

	if (switch_true(event_lock_pri)) {
		switch_channel_set_flag_recursive(channel, CF_EVENT_LOCK_PRI);
		elp = 1;
	}

	if (lead_frames) {
		switch_frame_t *read_frame;
		int frame_count = atoi(lead_frames);
		int max_frames = frame_count * 2;

		while (frame_count > 0 && --max_frames > 0) {
			status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				goto done;
			}
			if (!switch_test_flag(read_frame, SFF_CNG)) {
				frame_count--;
			}
		}
	}

	if (cmd_hash == CMD_EXECUTE) {
		char *app_name = switch_event_get_header(event, "execute-app-name");
		char *app_arg = switch_event_get_header(event, "execute-app-arg");
		char *content_type = switch_event_get_header(event, "content-type");
		char *loop_h = switch_event_get_header(event, "loops");
		char *hold_bleg = switch_event_get_header(event, "hold-bleg");
		int loops = 1;

		if (zstr(app_arg) && !zstr(content_type) && !strcasecmp(content_type, "text/plain")) {
			app_arg = switch_event_get_body(event);
		}

		if (loop_h) {
			loops = atoi(loop_h);
		}

		if (app_name) {
			int x;
			const char *b_uuid = NULL;
			switch_core_session_t *b_session = NULL;

			switch_channel_clear_flag(channel, CF_STOP_BROADCAST);
			switch_channel_set_flag(channel, CF_BROADCAST);
			if (hold_bleg && switch_true(hold_bleg)) {
				if ((b_uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))) {
					const char *stream;
					b_uuid = switch_core_session_strdup(session, b_uuid);

					if (!(stream = switch_channel_get_variable_partner(channel, SWITCH_HOLD_MUSIC_VARIABLE))) {
						stream = switch_channel_get_variable(channel, SWITCH_HOLD_MUSIC_VARIABLE);
					}

					if (stream && switch_is_moh(stream)) {
						if ((b_session = switch_core_session_locate(b_uuid))) {
							switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
							switch_ivr_broadcast(b_uuid, stream, SMF_ECHO_ALEG | SMF_LOOP);
							switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_TRUE, 5000, NULL);
							switch_core_session_rwunlock(b_session);
						}
					} else {
						b_uuid = NULL;
					}
				}
			}

			for (x = 0; x < loops || loops < 0; x++) {
				switch_time_t b4, aftr;

				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "%s Command Execute %s(%s)\n",
								  switch_channel_get_name(channel), app_name, switch_str_nil(app_arg));
				b4 = switch_micro_time_now();
				if (switch_core_session_execute_application(session, app_name, app_arg) != SWITCH_STATUS_SUCCESS) {
					goto done;
				}

				aftr = switch_micro_time_now();
				if (!switch_channel_ready(channel) || switch_channel_test_flag(channel, CF_STOP_BROADCAST) || aftr - b4 < 500000) {
					break;
				}
			}

			if (b_uuid) {
				if ((b_session = switch_core_session_locate(b_uuid))) {
					switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
					switch_channel_stop_broadcast(b_channel);
					switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
					switch_core_session_rwunlock(b_session);
				}
			}

			switch_channel_clear_flag(channel, CF_BROADCAST);
		}
	} else if (cmd_hash == CMD_UNICAST) {
		char *local_ip = switch_event_get_header(event, "local-ip");
		char *local_port = switch_event_get_header(event, "local-port");
		char *remote_ip = switch_event_get_header(event, "remote-ip");
		char *remote_port = switch_event_get_header(event, "remote-port");
		char *transport = switch_event_get_header(event, "transport");
		char *flags = switch_event_get_header(event, "flags");

		if (zstr(local_ip)) {
			local_ip = "127.0.0.1";
		}
		if (zstr(remote_ip)) {
			remote_ip = "127.0.0.1";
		}
		if (zstr(local_port)) {
			local_port = "8025";
		}
		if (zstr(remote_port)) {
			remote_port = "8026";
		}
		if (zstr(transport)) {
			transport = "udp";
		}

		switch_ivr_activate_unicast(session, local_ip, (switch_port_t) atoi(local_port), remote_ip, (switch_port_t) atoi(remote_port), transport, flags);

	} else if (cmd_hash == CMD_HANGUP) {
		char *cause_name = switch_event_get_header(event, "hangup-cause");
		switch_call_cause_t cause = SWITCH_CAUSE_NORMAL_CLEARING;

		if (cause_name) {
			cause = switch_channel_str2cause(cause_name);
		}

		switch_channel_hangup(channel, cause);
	} else if (cmd_hash == CMD_NOMEDIA) {
		char *uuid = switch_event_get_header(event, "nomedia-uuid");
		switch_ivr_nomedia(uuid, SMF_REBRIDGE);
	}

	status = SWITCH_STATUS_SUCCESS;

  done:

	switch_channel_clear_flag_recursive(channel, CF_EVENT_PARSE);

	if (el) {
		switch_channel_clear_flag_recursive(channel, CF_EVENT_LOCK);
	}

	if (elp) {
		switch_channel_clear_flag_recursive(channel, CF_EVENT_LOCK_PRI);
	}

	return switch_channel_test_flag(channel, CF_BREAK) ? SWITCH_STATUS_BREAK : status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_parse_next_event(switch_core_session_t *session)
{
	switch_event_t *event;
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (switch_core_session_dequeue_private_event(session, &event) == SWITCH_STATUS_SUCCESS) {
		status = switch_ivr_parse_event(session, event);
		switch_event_fire(&event);
	}

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_parse_all_messages(switch_core_session_t *session)
{
	switch_core_session_message_t *message;
	int i = 0;

	while (switch_core_session_dequeue_message(session, &message) == SWITCH_STATUS_SUCCESS) {
		i++;
		switch_core_session_receive_message(session, message);
		message = NULL;
	}

	return i ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_parse_all_events(switch_core_session_t *session)
{
	int x = 0;


	switch_ivr_parse_all_messages(session);

	while (switch_ivr_parse_next_event(session) == SWITCH_STATUS_SUCCESS)
		x++;

	if (x) {
		switch_ivr_sleep(session, 0, SWITCH_TRUE, NULL);
	}

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_park(switch_core_session_t *session, switch_input_args_t *args)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_frame_t *read_frame = NULL;
	int stream_id = 0;
	switch_event_t *event;
	switch_unicast_conninfo_t *conninfo = NULL;
	uint32_t rate = 0;
	uint32_t bpf = 0;
	const char *to;
	int timeout = 0;
	time_t expires = 0;
	switch_codec_implementation_t read_impl = { 0 };
	switch_call_cause_t timeout_cause = SWITCH_CAUSE_NORMAL_CLEARING;


	if (switch_channel_test_flag(channel, CF_CONTROLLED)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot park channels that are under control already.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_test_flag(channel, CF_PROXY_MODE) || switch_channel_get_state(channel) == CS_RESET) {
		return SWITCH_STATUS_FALSE;
	}

	if ((to = switch_channel_get_variable(channel, "park_timeout"))) {
		char *cause_str;

		if ((cause_str = strchr(to, ':'))) {
			timeout_cause = switch_channel_str2cause(cause_str + 1);
		}
		
		if ((timeout = atoi(to)) < 0) {
			timeout = 0;
		} else {
			expires = switch_epoch_time_now(NULL) + timeout;
		}
		switch_channel_set_variable(channel, "park_timeout", NULL);
		switch_channel_set_variable(channel, SWITCH_PARK_AFTER_BRIDGE_VARIABLE, NULL);
	}

	switch_channel_set_flag(channel, CF_CONTROLLED);
	switch_channel_set_flag(channel, CF_PARK);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_PARK) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}

	while (switch_channel_ready(channel) && switch_channel_test_flag(channel, CF_CONTROLLED) && switch_channel_test_flag(channel, CF_PARK)) {

		if (!rate && switch_channel_media_ready(channel)) {
			switch_core_session_get_read_impl(session, &read_impl);
			rate = read_impl.actual_samples_per_second;
			bpf = read_impl.decoded_bytes_per_packet;
		}

		if (rate) {
			status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, stream_id);
		} else {
			switch_yield(20000);
			status = SWITCH_STATUS_SUCCESS;
		}

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		} else {
			if (expires && switch_epoch_time_now(NULL) >= expires) {
				switch_channel_hangup(channel, timeout_cause);
				break;
			}

			if (switch_channel_test_flag(channel, CF_UNICAST)) {
				if (!switch_channel_media_ready(channel)) {
					if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
						return SWITCH_STATUS_FALSE;
					}
				}

				if (!conninfo) {
					if (!(conninfo = switch_channel_get_private(channel, "unicast"))) {
						switch_channel_clear_flag(channel, CF_UNICAST);
					}

					if (conninfo) {
						unicast_thread_launch(conninfo);
					}
				}

				if (conninfo) {
					switch_size_t len = 0;
					uint32_t flags = 0;
					switch_byte_t decoded[SWITCH_RECOMMENDED_BUFFER_SIZE];
					uint32_t dlen = sizeof(decoded);
					switch_status_t tstatus;
					switch_byte_t *sendbuf = NULL;
					uint32_t sendlen = 0;

					switch_assert(read_frame);

					if (switch_test_flag(read_frame, SFF_CNG)) {
						sendlen = bpf;
						switch_assert(sendlen <= SWITCH_RECOMMENDED_BUFFER_SIZE);
						memset(decoded, 255, sendlen);
						sendbuf = decoded;
						tstatus = SWITCH_STATUS_SUCCESS;
					} else {
						if (switch_test_flag(conninfo, SUF_NATIVE)) {
							tstatus = SWITCH_STATUS_NOOP;
						} else {
							switch_codec_t *read_codec = switch_core_session_get_read_codec(session);
							tstatus = switch_core_codec_decode(read_codec,
															   &conninfo->read_codec,
															   read_frame->data,
															   read_frame->datalen, read_impl.actual_samples_per_second, decoded, &dlen, &rate, &flags);
						}
						switch (tstatus) {
						case SWITCH_STATUS_NOOP:
						case SWITCH_STATUS_BREAK:
							sendbuf = read_frame->data;
							sendlen = read_frame->datalen;
							tstatus = SWITCH_STATUS_SUCCESS;
							break;
						case SWITCH_STATUS_SUCCESS:
							sendbuf = decoded;
							sendlen = dlen;
							tstatus = SWITCH_STATUS_SUCCESS;
							break;
						default:
							switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Codec Error\n");
							switch_ivr_deactivate_unicast(session);
							break;
						}
					}

					if (tstatus == SWITCH_STATUS_SUCCESS) {
						len = sendlen;
						if (switch_socket_sendto(conninfo->socket, conninfo->remote_addr, 0, (void *) sendbuf, &len) != SWITCH_STATUS_SUCCESS) {
							switch_ivr_deactivate_unicast(session);
						}
					}
				}
			}

			switch_ivr_parse_all_events(session);


			if (switch_channel_has_dtmf(channel)) {
				switch_dtmf_t dtmf = { 0 };
				switch_channel_dequeue_dtmf(channel, &dtmf);
				if (args && args->input_callback) {
					if ((status = args->input_callback(session, (void *) &dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}

			if (switch_core_session_dequeue_event(session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
				if (args && args->input_callback) {
					if ((status = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				} else {
					switch_channel_event_set_data(channel, event);
					switch_event_fire(&event);
				}
			}
		}

	}

	switch_channel_clear_flag(channel, CF_CONTROLLED);
	switch_channel_clear_flag(channel, CF_PARK);

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNPARK) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
	}

	if (switch_channel_test_flag(channel, CF_UNICAST)) {
		switch_ivr_deactivate_unicast(session);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_callback(switch_core_session_t *session, switch_input_args_t *args, uint32_t digit_timeout,
																   uint32_t abs_timeout)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_time_t abs_started = 0, digit_started = 0;
	uint32_t abs_elapsed = 0, digit_elapsed = 0;

	if (!args || !args->input_callback) {
		return SWITCH_STATUS_GENERR;
	}

	if (abs_timeout) {
		abs_started = switch_micro_time_now();
	}
	if (digit_timeout) {
		digit_started = switch_micro_time_now();
	}
	while (switch_channel_ready(channel)) {
		switch_frame_t *read_frame = NULL;
		switch_event_t *event;
		switch_dtmf_t dtmf = { 0 };

		if (switch_channel_test_flag(channel, CF_BREAK)) {
			switch_channel_clear_flag(channel, CF_BREAK);
			status = SWITCH_STATUS_BREAK;
			break;
		}

		if (abs_timeout) {
			abs_elapsed = (uint32_t) ((switch_micro_time_now() - abs_started) / 1000);
			if (abs_elapsed >= abs_timeout) {
				status = SWITCH_STATUS_TIMEOUT;
				break;
			}
		}
		if (digit_timeout) {
			digit_elapsed = (uint32_t) ((switch_micro_time_now() - digit_started) / 1000);
			if (digit_elapsed >= digit_timeout) {
				status = SWITCH_STATUS_TIMEOUT;
				break;
			}
		}


		switch_ivr_parse_all_events(session);


		if (switch_channel_has_dtmf(channel)) {
			switch_channel_dequeue_dtmf(channel, &dtmf);
			status = args->input_callback(session, (void *) &dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen);
			if (digit_timeout) {
				digit_started = switch_micro_time_now();
			}
		}

		if (switch_core_session_dequeue_event(session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			status = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);
			switch_event_destroy(&event);
		}

		if (status != SWITCH_STATUS_SUCCESS) {
			break;
		}

		if (switch_channel_test_flag(channel, CF_SERVICE)) {
			switch_cond_next();
		} else {
			status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		}

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (read_frame && args && (args->read_frame_callback)) {
			if (args->read_frame_callback(session, read_frame, args->user_data) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_collect_digits_count(switch_core_session_t *session,
																char *buf,
																switch_size_t buflen,
																switch_size_t maxdigits,
																const char *terminators, char *terminator,
																uint32_t first_timeout, uint32_t digit_timeout, uint32_t abs_timeout)
{
	switch_size_t i = 0, x = strlen(buf);
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_time_t started = 0, digit_started = 0;
	uint32_t abs_elapsed = 0, digit_elapsed = 0;
	uint32_t eff_timeout = 0;
	switch_frame_t write_frame = { 0 };
	unsigned char *abuf = NULL;
	switch_codec_implementation_t imp = { 0 };
	switch_codec_t codec = { 0 };
	int sval = 0;
	const char *var;

	if ((var = switch_channel_get_variable(channel, SWITCH_SEND_SILENCE_WHEN_IDLE_VARIABLE)) && (sval = atoi(var))) {
		switch_core_session_get_read_impl(session, &imp);

		if (switch_core_codec_init(&codec,
								   "L16",
								   NULL,
								   imp.samples_per_second,
								   imp.microseconds_per_packet / 1000,
								   imp.number_of_channels,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
								   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Error L16@%uhz %u channels %dms\n",
							  imp.samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);
			return SWITCH_STATUS_FALSE;
		}


		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Codec Activated L16@%uhz %u channels %dms\n",
						  imp.samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);

		write_frame.codec = &codec;
		switch_zmalloc(abuf, SWITCH_RECOMMENDED_BUFFER_SIZE);
		write_frame.data = abuf;
		write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;
		write_frame.datalen = imp.decoded_bytes_per_packet;
		write_frame.samples = write_frame.datalen / sizeof(int16_t);
	}

	if (terminator != NULL) {
		*terminator = '\0';
	}

	if (!zstr(terminators)) {
		for (i = 0; i < x; i++) {
			if (strchr(terminators, buf[i]) && terminator != NULL) {
				*terminator = buf[i];
				buf[i] = '\0';
				return SWITCH_STATUS_SUCCESS;
			}
		}
	}

	if (abs_timeout) {
		started = switch_micro_time_now();
	}

	if (digit_timeout && first_timeout) {
		eff_timeout = first_timeout;
	} else if (digit_timeout && !first_timeout) {
		first_timeout = eff_timeout = digit_timeout;
	} else if (first_timeout) {
		digit_timeout = eff_timeout = first_timeout;
	}


	if (eff_timeout) {
		digit_started = switch_micro_time_now();
	}

	while (switch_channel_ready(channel)) {
		switch_frame_t *read_frame;

		if (abs_timeout) {
			abs_elapsed = (uint32_t) ((switch_micro_time_now() - started) / 1000);
			if (abs_elapsed >= abs_timeout) {
				status = SWITCH_STATUS_TIMEOUT;
				break;
			}
		}


		switch_ivr_parse_all_events(session);



		if (eff_timeout) {
			digit_elapsed = (uint32_t) ((switch_micro_time_now() - digit_started) / 1000);

			if (digit_elapsed >= eff_timeout) {
				status = SWITCH_STATUS_TIMEOUT;
				break;
			}
		}

		if (switch_channel_has_dtmf(channel)) {
			switch_dtmf_t dtmf = { 0 };
			switch_size_t y;

			if (eff_timeout) {
				eff_timeout = digit_timeout;
				digit_started = switch_micro_time_now();
			}

			for (y = 0; y <= maxdigits; y++) {
				if (switch_channel_dequeue_dtmf(channel, &dtmf) != SWITCH_STATUS_SUCCESS) {
					break;
				}

				if (!zstr(terminators) && strchr(terminators, dtmf.digit) && terminator != NULL) {
					*terminator = dtmf.digit;
					return SWITCH_STATUS_SUCCESS;
				}


				buf[x++] = dtmf.digit;
				buf[x] = '\0';

				if (x >= buflen || x >= maxdigits) {
					return SWITCH_STATUS_SUCCESS;
				}
			}
		}

		if (switch_channel_test_flag(channel, CF_SERVICE)) {
			switch_cond_next();
		} else {
			status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}

			if (write_frame.data) {
				switch_generate_sln_silence((int16_t *) write_frame.data, write_frame.samples, sval);
				switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
			}

		}
	}

	if (write_frame.codec) {
		switch_core_codec_destroy(&codec);
	}

	switch_safe_free(abuf);

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_hold(switch_core_session_t *session, const char *message, switch_bool_t moh)
{
	switch_core_session_message_t msg = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *stream;
	const char *other_uuid;

	msg.message_id = SWITCH_MESSAGE_INDICATE_HOLD;
	msg.string_arg = message;
	msg.from = __FILE__;

	switch_channel_set_flag(channel, CF_HOLD);
	switch_channel_set_flag(channel, CF_SUSPEND);

	switch_core_session_receive_message(session, &msg);

	if (moh && (stream = switch_channel_get_variable(channel, SWITCH_HOLD_MUSIC_VARIABLE))) {
		if ((other_uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE))) {
			switch_ivr_broadcast(other_uuid, stream, SMF_ECHO_ALEG | SMF_LOOP);
		}
	}


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_hold_uuid(const char *uuid, const char *message, switch_bool_t moh)
{
	switch_core_session_t *session;

	if ((session = switch_core_session_locate(uuid))) {
		switch_ivr_hold(session, message, moh);
		switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_unhold(switch_core_session_t *session)
{
	switch_core_session_message_t msg = { 0 };
	switch_channel_t *channel = switch_core_session_get_channel(session);
	const char *other_uuid;
	switch_core_session_t *b_session;

	msg.message_id = SWITCH_MESSAGE_INDICATE_UNHOLD;
	msg.from = __FILE__;

	switch_channel_clear_flag(channel, CF_HOLD);
	switch_channel_clear_flag(channel, CF_SUSPEND);

	switch_core_session_receive_message(session, &msg);


	if ((other_uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE)) && (b_session = switch_core_session_locate(other_uuid))) {
		switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
		switch_channel_stop_broadcast(b_channel);
		switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
		switch_core_session_rwunlock(b_session);
	}


	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_unhold_uuid(const char *uuid)
{
	switch_core_session_t *session;

	if ((session = switch_core_session_locate(uuid))) {
		switch_ivr_unhold(session);
		switch_core_session_rwunlock(session);
	}

	return SWITCH_STATUS_SUCCESS;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_media(const char *uuid, switch_media_flag_t flags)
{
	const char *other_uuid = NULL;
	switch_channel_t *channel, *other_channel = NULL;
	switch_core_session_t *session, *other_session;
	switch_core_session_message_t msg = { 0 };
	switch_status_t status = SWITCH_STATUS_GENERR;
	uint8_t swap = 0;
	switch_frame_t *read_frame = NULL;

	msg.message_id = SWITCH_MESSAGE_INDICATE_MEDIA;
	msg.from = __FILE__;

	if ((session = switch_core_session_locate(uuid))) {
		channel = switch_core_session_get_channel(session);

		if ((flags & SMF_REBRIDGE) && !switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR)) {
			swap = 1;
		}

		if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
			status = SWITCH_STATUS_SUCCESS;
			switch_core_session_receive_message(session, &msg);

			if ((flags & SMF_IMMEDIATE)) {
				switch_channel_wait_for_flag(channel, CF_REQ_MEDIA, SWITCH_FALSE, 250, NULL);
				switch_yield(250000);
			} else {
				switch_status_t st;
				switch_channel_wait_for_flag(channel, CF_REQ_MEDIA, SWITCH_FALSE, 10000, NULL);
				switch_channel_wait_for_flag(channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);
				switch_channel_wait_for_flag(channel, CF_MEDIA_SET, SWITCH_TRUE, 10000, NULL);
				st = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
			}

			if ((flags & SMF_REBRIDGE)
				&& (other_uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE))
				&& (other_session = switch_core_session_locate(other_uuid))) {
				other_channel = switch_core_session_get_channel(other_session);
				switch_assert(other_channel != NULL);
				switch_core_session_receive_message(other_session, &msg);
				switch_channel_wait_for_flag(other_channel, CF_REQ_MEDIA, SWITCH_FALSE, 10000, NULL);
				switch_channel_wait_for_flag(other_channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);
				switch_channel_wait_for_flag(other_channel, CF_MEDIA_SET, SWITCH_TRUE, 10000, NULL);
				switch_core_session_read_frame(other_session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
				switch_channel_clear_state_handler(other_channel, NULL);
				switch_core_session_rwunlock(other_session);
			}
			if (other_channel) {
				switch_channel_clear_state_handler(channel, NULL);
			}
		}

		switch_core_session_rwunlock(session);

		if (other_channel) {
			if (swap) {
				switch_ivr_uuid_bridge(other_uuid, uuid);
			} else {
				switch_ivr_uuid_bridge(uuid, other_uuid);
			}
			switch_channel_wait_for_flag(channel, CF_BRIDGED, SWITCH_TRUE, 1000, NULL);
			switch_channel_wait_for_flag(other_channel, CF_BRIDGED, SWITCH_TRUE, 1000, NULL);
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_nomedia(const char *uuid, switch_media_flag_t flags)
{
	const char *other_uuid;
	switch_channel_t *channel, *other_channel = NULL;
	switch_core_session_t *session, *other_session = NULL;
	switch_core_session_message_t msg = { 0 };
	switch_status_t status = SWITCH_STATUS_GENERR;
	uint8_t swap = 0;

	msg.message_id = SWITCH_MESSAGE_INDICATE_NOMEDIA;
	msg.from = __FILE__;

	if ((session = switch_core_session_locate(uuid))) {
		status = SWITCH_STATUS_SUCCESS;
		channel = switch_core_session_get_channel(session);

		if ((flags & SMF_REBRIDGE) && !switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR)) {
			swap = 1;
		}

		switch_channel_set_flag(channel, CF_REDIRECT);
		switch_channel_set_flag(channel, CF_RESET);

		if ((flags & SMF_FORCE) || !switch_channel_test_flag(channel, CF_PROXY_MODE)) {
			if ((flags & SMF_REBRIDGE) && (other_uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE)) &&
				(other_session = switch_core_session_locate(other_uuid))) {
				other_channel = switch_core_session_get_channel(other_session);

				switch_channel_set_flag(other_channel, CF_RESET);
				switch_channel_set_flag(other_channel, CF_REDIRECT);
				switch_channel_set_state(other_channel, CS_PARK);
				if (switch_core_session_in_thread(session)) {
					switch_yield(100000);
				} else {
					switch_channel_wait_for_state(other_channel, channel, CS_PARK);
				}
				switch_core_session_receive_message(other_session, &msg);
				switch_channel_wait_for_flag(other_channel, CF_REQ_MEDIA, SWITCH_FALSE, 10000, NULL);
				switch_channel_wait_for_flag(other_channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);
				switch_channel_wait_for_flag(other_channel, CF_MEDIA_SET, SWITCH_TRUE, 10000, NULL);
			}

			switch_core_session_receive_message(session, &msg);

			if (!switch_core_session_in_thread(session)) {
				switch_channel_set_state(channel, CS_PARK);
				switch_channel_wait_for_state(channel, channel, CS_PARK);
				switch_channel_wait_for_flag(channel, CF_REQ_MEDIA, SWITCH_FALSE, 10000, NULL);
				switch_channel_wait_for_flag(channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);
				switch_channel_wait_for_flag(channel, CF_MEDIA_SET, SWITCH_TRUE, 10000, NULL);
			}

			if (other_channel) {
				if (swap) {
					switch_ivr_signal_bridge(other_session, session);
				} else {
					switch_ivr_signal_bridge(session, other_session);
				}

				if (switch_core_session_in_thread(session)) {
                    switch_yield(100000);
                } else {
					switch_channel_wait_for_state(other_channel, channel, CS_HIBERNATE);
				}

				if (!switch_core_session_in_thread(session)) {
					switch_channel_wait_for_state(channel, other_channel, CS_HIBERNATE);
				}
				switch_core_session_rwunlock(other_session);
			}
		}
		switch_core_session_rwunlock(session);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_session_transfer(switch_core_session_t *session, const char *extension, const char *dialplan,
															const char *context)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *profile, *new_profile;
	switch_core_session_message_t msg = { 0 };
	switch_core_session_t *other_session;
	switch_channel_t *other_channel = NULL;
	const char *uuid = NULL;
	const char *max_forwards;
	const char *forwardvar = switch_channel_get_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE);
	int forwardval = 70;

	if (!zstr(forwardvar)) {
		forwardval = atoi(forwardvar) - 1;
	}
	if (forwardval <= 0) {
		switch_channel_hangup(channel, SWITCH_CAUSE_EXCHANGE_ROUTING_ERROR);
		return SWITCH_STATUS_FALSE;
	}

	max_forwards = switch_core_session_sprintf(session, "%d", forwardval);
	switch_channel_set_variable(channel, SWITCH_MAX_FORWARDS_VARIABLE, max_forwards);

	switch_core_session_reset(session, SWITCH_TRUE, SWITCH_TRUE);
	switch_channel_clear_flag(channel, CF_ORIGINATING);

	/* clear all state handlers */
	switch_channel_clear_state_handler(channel, NULL);

	if ((profile = switch_channel_get_caller_profile(channel))) {
		const char *var;

		if (zstr(dialplan)) {
			dialplan = profile->dialplan;
			if (!zstr(dialplan) && !strcasecmp(dialplan, "inline")) {
				dialplan = NULL;
			}
		}

		if (zstr(context)) {
			context = profile->context;
		}

		if (zstr(dialplan)) {
			dialplan = "XML";
		}

		if (zstr(context)) {
			context = "default";
		}

		if (zstr(extension)) {
			extension = "service";
		}

		if ((var = switch_channel_get_variable(channel, "force_transfer_dialplan"))) {
			dialplan = var;
		}

		if ((var = switch_channel_get_variable(channel, "force_transfer_context"))) {
			context = var;
		}

		new_profile = switch_caller_profile_clone(session, profile);

		new_profile->dialplan = switch_core_strdup(new_profile->pool, dialplan);
		new_profile->context = switch_core_strdup(new_profile->pool, context);
		new_profile->destination_number = switch_core_strdup(new_profile->pool, extension);
		new_profile->rdnis = switch_core_strdup(new_profile->pool, profile->destination_number);

		switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, NULL);

		/* If HANGUP_AFTER_BRIDGE is set to 'true', SWITCH_SIGNAL_BRIDGE_VARIABLE 
		 * will not have a value, so we need to check SWITCH_BRIDGE_VARIABLE */

		uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE);

		if (!uuid) {
			uuid = switch_channel_get_variable(channel, SWITCH_BRIDGE_VARIABLE);
		}

		if (uuid && (other_session = switch_core_session_locate(uuid))) {
			other_channel = switch_core_session_get_channel(other_session);
			switch_channel_set_variable(other_channel, SWITCH_SIGNAL_BOND_VARIABLE, NULL);
			switch_core_session_rwunlock(other_session);
		}

		if ((uuid = switch_channel_get_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE))
			&& (other_session = switch_core_session_locate(uuid))) {
			other_channel = switch_core_session_get_channel(other_session);

			switch_channel_set_variable(channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);
			switch_channel_set_variable(other_channel, SWITCH_SIGNAL_BRIDGE_VARIABLE, NULL);

			switch_channel_set_variable(channel, SWITCH_BRIDGE_VARIABLE, NULL);
			switch_channel_set_variable(other_channel, SWITCH_BRIDGE_VARIABLE, NULL);

			/* If we are transferring the CALLER out of the bridge, we do not want to hang up on them */
			switch_channel_set_variable(channel, SWITCH_HANGUP_AFTER_BRIDGE_VARIABLE, "false");

			switch_channel_hangup(other_channel, SWITCH_CAUSE_BLIND_TRANSFER);
			switch_ivr_media(uuid, SMF_NONE);

			switch_core_session_rwunlock(other_session);
		}

		switch_channel_set_caller_profile(channel, new_profile);
		switch_channel_set_flag(channel, CF_TRANSFER);

		switch_channel_set_state(channel, CS_ROUTING);

		msg.message_id = SWITCH_MESSAGE_INDICATE_TRANSFER;
		msg.from = __FILE__;
		switch_core_session_receive_message(session, &msg);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Transfer %s to %s[%s@%s]\n", switch_channel_get_name(channel), dialplan,
						  extension, context);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_transfer_variable(switch_core_session_t *sessa, switch_core_session_t *sessb, char *var)
{
	switch_channel_t *chana = switch_core_session_get_channel(sessa);
	switch_channel_t *chanb = switch_core_session_get_channel(sessb);
	const char *val = NULL;
	uint8_t prefix = 0;

	if (var && *var == '~') {
		var++;
		prefix = 1;
	}

	if (var && !prefix) {
		if ((val = switch_channel_get_variable(chana, var))) {
			switch_channel_set_variable(chanb, var, val);
		}
	} else {
		switch_event_header_t *hi;
		if ((hi = switch_channel_variable_first(chana))) {
			for (; hi; hi = hi->next) {
				char *vvar = hi->name;
				char *vval = hi->value;
				if (vvar && vval && (!prefix || (var && !strncmp((char *) vvar, var, strlen(var))))) {
					switch_channel_set_variable(chanb, (char *) vvar, (char *) vval);
				}
			}
			switch_channel_variable_last(chana);
		}
	}

	return SWITCH_STATUS_SUCCESS;
}

/******************************************************************************************************/

struct switch_ivr_digit_stream_parser {
	int pool_auto_created;
	switch_memory_pool_t *pool;
	switch_hash_t *hash;
	switch_size_t maxlen;
	switch_size_t buflen;
	switch_size_t minlen;
	char terminator;
	unsigned int digit_timeout_ms;
};

struct switch_ivr_digit_stream {
	char *digits;
	switch_time_t last_digit_time;
};

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_new(switch_memory_pool_t *pool, switch_ivr_digit_stream_parser_t ** parser)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL) {
		int pool_auto_created = 0;

		/* if the caller didn't provide a pool, make one */
		if (pool == NULL) {
			switch_core_new_memory_pool(&pool);
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "created a memory pool\n");
			if (pool != NULL) {
				pool_auto_created = 1;
			}
		}
		/* if we have a pool, make a parser object */
		if (pool != NULL) {
			*parser = (switch_ivr_digit_stream_parser_t *) switch_core_alloc(pool, sizeof(switch_ivr_digit_stream_parser_t));
		}
		/* if we have parser object, initialize it for the caller */
		if (pool && *parser != NULL) {
			memset(*parser, 0, sizeof(switch_ivr_digit_stream_parser_t));
			(*parser)->pool_auto_created = pool_auto_created;
			(*parser)->pool = pool;
			(*parser)->digit_timeout_ms = 1000;
			switch_core_hash_init(&(*parser)->hash, (*parser)->pool);

			status = SWITCH_STATUS_SUCCESS;
		} else {
			status = SWITCH_STATUS_MEMERR;
			/* if we can't create a parser object,clean up the pool if we created it */
			if (pool != NULL && pool_auto_created) {
				switch_core_destroy_memory_pool(&pool);
			}
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_destroy(switch_ivr_digit_stream_parser_t *parser)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL) {
		if (parser->hash != NULL) {
			switch_core_hash_destroy(&parser->hash);
			parser->hash = NULL;
		}
		/* free the memory pool if we created it */
		if (parser->pool_auto_created && parser->pool != NULL) {
			status = switch_core_destroy_memory_pool(&parser->pool);
		}
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_new(switch_ivr_digit_stream_parser_t *parser, switch_ivr_digit_stream_t ** stream)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	/* if we have a parser object memory pool and a stream object pointer that is null */
	if (parser && stream && *stream == NULL) {
		*stream = (switch_ivr_digit_stream_t *) malloc(sizeof(**stream));
		switch_assert(*stream);
		memset(*stream, 0, sizeof(**stream));
		switch_zmalloc((*stream)->digits, parser->buflen + 1);
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_destroy(switch_ivr_digit_stream_t ** stream)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (*stream) {
		switch_safe_free((*stream)->digits);
		free(*stream);
		*stream = NULL;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_set_event(switch_ivr_digit_stream_parser_t *parser, char *digits, void *data)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL && digits != NULL && *digits && parser->hash != NULL) {

		status = switch_core_hash_insert(parser->hash, digits, data);
		if (status == SWITCH_STATUS_SUCCESS) {
			switch_size_t len = strlen(digits);

			/* if we don't have a terminator, then we have to try and
			 * figure out when a digit set is completed, therefore we
			 * keep track of the min and max digit lengths
			 */

			if (len > parser->buflen) {
				parser->buflen = len;
			}

			if (parser->terminator == '\0') {
				if (len > parser->maxlen) {
					parser->maxlen = len;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "max len %u\n", (uint32_t) parser->maxlen);
				}
				if (parser->minlen == 0 || len < parser->minlen) {
					parser->minlen = len;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "min len %u\n", (uint32_t) parser->minlen);
				}
			} else {
				/* since we have a terminator, reset min and max */
				parser->minlen = 0;
				parser->maxlen = 0;
			}
		}
	}
	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unable to add hash for '%s'\n", digits);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_del_event(switch_ivr_digit_stream_parser_t *parser, char *digits)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL && digits != NULL && *digits) {
		status = switch_core_hash_delete(parser->hash, digits);
	}

	if (status != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "unable to del hash for '%s'\n", digits);
	}

	return status;
}

SWITCH_DECLARE(void *) switch_ivr_digit_stream_parser_feed(switch_ivr_digit_stream_parser_t *parser, switch_ivr_digit_stream_t *stream, char digit)
{
	void *result = NULL;
	switch_size_t len;

	switch_assert(parser);
	switch_assert(stream);
	switch_assert(stream->digits);

	len = strlen(stream->digits);

	/* handle new digit arrivals */
	if (digit) {
		/* if it's not a terminator digit, add it to the collected digits */
		if (digit != parser->terminator) {
			/* if collected digits length >= the max length of the keys
			 * in the hash table, then left shift the digit string
			 */
			if (len > 0 && parser->maxlen != 0 && len >= parser->maxlen) {
				char *src = stream->digits + 1;
				char *dst = stream->digits;

				while (*src) {
					*(dst++) = *(src++);
				}
				*dst = digit;
			} else {
				*(stream->digits + (len++)) = digit;
				*(stream->digits + len) = '\0';
				stream->last_digit_time = switch_micro_time_now() / 1000;
			}
		}
	}

	/* don't allow collected digit string testing if there are varying sized keys until timeout */
	if (parser->maxlen - parser->minlen > 0 && (switch_micro_time_now() / 1000) - stream->last_digit_time < parser->digit_timeout_ms) {
		len = 0;
	}
	/* if we have digits to test */
	if (len) {
		result = switch_core_hash_find(parser->hash, stream->digits);
		/* if we matched the digit string, or this digit is the terminator
		 * reset the collected digits for next digit string
		 */
		if (result != NULL || parser->terminator == digit) {
			*stream->digits = '\0';
		}
	}


	return result;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_reset(switch_ivr_digit_stream_t *stream)
{
	switch_status_t status = SWITCH_STATUS_FALSE;
	switch_assert(stream);
	switch_assert(stream->digits);

	*stream->digits = '\0';
	stream->last_digit_time = 0;
	status = SWITCH_STATUS_SUCCESS;

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_digit_stream_parser_set_terminator(switch_ivr_digit_stream_parser_t *parser, char digit)
{
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (parser != NULL) {
		parser->terminator = digit;
		/* since we have a terminator, reset min and max */
		parser->minlen = 0;
		parser->maxlen = 0;
		status = SWITCH_STATUS_SUCCESS;
	}

	return status;
}

SWITCH_DECLARE(int) switch_ivr_set_xml_profile_data(switch_xml_t xml, switch_caller_profile_t *caller_profile, int off)
{
	switch_xml_t param;

	if (!(param = switch_xml_add_child_d(xml, "username", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->username);

	if (!(param = switch_xml_add_child_d(xml, "dialplan", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->dialplan);

	if (!(param = switch_xml_add_child_d(xml, "caller_id_name", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->caller_id_name);

	if (!(param = switch_xml_add_child_d(xml, "ani", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->ani);

	if (!(param = switch_xml_add_child_d(xml, "aniii", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->aniii);

	if (!(param = switch_xml_add_child_d(xml, "caller_id_number", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->caller_id_number);

	if (!(param = switch_xml_add_child_d(xml, "network_addr", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->network_addr);

	if (!(param = switch_xml_add_child_d(xml, "rdnis", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->rdnis);

	if (!(param = switch_xml_add_child_d(xml, "destination_number", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->destination_number);

	if (!(param = switch_xml_add_child_d(xml, "uuid", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->uuid);

	if (!(param = switch_xml_add_child_d(xml, "source", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->source);

	if (!(param = switch_xml_add_child_d(xml, "context", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->context);

	if (!(param = switch_xml_add_child_d(xml, "chan_name", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->chan_name);

	return off;
}

SWITCH_DECLARE(int) switch_ivr_set_xml_chan_vars(switch_xml_t xml, switch_channel_t *channel, int off)
{
	switch_xml_t variable;
	switch_event_header_t *hi = switch_channel_variable_first(channel);

	if (!hi)
		return off;

	for (; hi; hi = hi->next) {
		if (!zstr(hi->name) && !zstr(hi->value) && ((variable = switch_xml_add_child_d(xml, hi->name, off++)))) {
			char *data;
			switch_size_t dlen = strlen(hi->value) * 3;

			if ((data = malloc(dlen))) {
				memset(data, 0, dlen);
				switch_url_encode(hi->value, data, dlen);
				switch_xml_set_txt_d(variable, data);
				free(data);
			}
		}
	}
	switch_channel_variable_last(channel);

	return off;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_generate_xml_cdr(switch_core_session_t *session, switch_xml_t *xml_cdr)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	switch_xml_t variables, cdr, x_main_cp, x_caller_profile, x_caller_extension, x_times, time_tag,
		x_application, x_callflow, x_inner_extension, x_apps, x_o, x_channel_data, x_field;
	switch_app_log_t *app_log;
	char tmp[512], *f;
	int cdr_off = 0, v_off = 0, cd_off = 0;

	if (!(cdr = switch_xml_new("cdr"))) {
		return SWITCH_STATUS_SUCCESS;
	}

	if (!(x_channel_data = switch_xml_add_child_d(cdr, "channel_data", cdr_off++))) {
		goto error;
	}

	x_field = switch_xml_add_child_d(x_channel_data, "state", cd_off++);
	switch_xml_set_txt_d(x_field, switch_channel_state_name(switch_channel_get_state(channel)));

	x_field = switch_xml_add_child_d(x_channel_data, "direction", cd_off++);
	switch_xml_set_txt_d(x_field, switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND ? "outbound" : "inbound");

	x_field = switch_xml_add_child_d(x_channel_data, "state_number", cd_off++);
	switch_snprintf(tmp, sizeof(tmp), "%d", switch_channel_get_state(channel));
	switch_xml_set_txt_d(x_field, tmp);

	if ((f = switch_channel_get_flag_string(channel))) {
		x_field = switch_xml_add_child_d(x_channel_data, "flags", cd_off++);
		switch_xml_set_txt_d(x_field, f);
		free(f);
	}

	if ((f = switch_channel_get_cap_string(channel))) {
		x_field = switch_xml_add_child_d(x_channel_data, "caps", cd_off++);
		switch_xml_set_txt_d(x_field, f);
		free(f);
	}


	if (!(variables = switch_xml_add_child_d(cdr, "variables", cdr_off++))) {
		goto error;
	}

	switch_ivr_set_xml_chan_vars(variables, channel, v_off);


	if ((app_log = switch_core_session_get_app_log(session))) {
		int app_off = 0;
		switch_app_log_t *ap;

		if (!(x_apps = switch_xml_add_child_d(cdr, "app_log", cdr_off++))) {
			goto error;
		}
		for (ap = app_log; ap; ap = ap->next) {

			if (!(x_application = switch_xml_add_child_d(x_apps, "application", app_off++))) {
				goto error;
			}

			switch_xml_set_attr_d(x_application, "app_name", ap->app);
			switch_xml_set_attr_d(x_application, "app_data", ap->arg);
		}
	}


	caller_profile = switch_channel_get_caller_profile(channel);

	while (caller_profile) {
		int cf_off = 0;
		int cp_off = 0;

		if (!(x_callflow = switch_xml_add_child_d(cdr, "callflow", cdr_off++))) {
			goto error;
		}

		if (!zstr(caller_profile->dialplan)) {
			switch_xml_set_attr_d(x_callflow, "dialplan", caller_profile->dialplan);
		}

		if (!zstr(caller_profile->profile_index)) {
			switch_xml_set_attr_d(x_callflow, "profile_index", caller_profile->profile_index);
		}

		if (caller_profile->caller_extension) {
			switch_caller_application_t *ap;
			int app_off = 0;

			if (!(x_caller_extension = switch_xml_add_child_d(x_callflow, "extension", cf_off++))) {
				goto error;
			}

			switch_xml_set_attr_d(x_caller_extension, "name", caller_profile->caller_extension->extension_name);
			switch_xml_set_attr_d(x_caller_extension, "number", caller_profile->caller_extension->extension_number);
			if (caller_profile->caller_extension->current_application) {
				switch_xml_set_attr_d(x_caller_extension, "current_app", caller_profile->caller_extension->current_application->application_name);
			}

			for (ap = caller_profile->caller_extension->applications; ap; ap = ap->next) {
				if (!(x_application = switch_xml_add_child_d(x_caller_extension, "application", app_off++))) {
					goto error;
				}
				if (ap == caller_profile->caller_extension->current_application) {
					switch_xml_set_attr_d(x_application, "last_executed", "true");
				}
				switch_xml_set_attr_d(x_application, "app_name", ap->application_name);
				switch_xml_set_attr_d(x_application, "app_data", ap->application_data);
			}

			if (caller_profile->caller_extension->children) {
				switch_caller_profile_t *cp = NULL;
				int i_off = 0, i_app_off = 0;
				for (cp = caller_profile->caller_extension->children; cp; cp = cp->next) {

					if (!cp->caller_extension) {
						continue;
					}
					if (!(x_inner_extension = switch_xml_add_child_d(x_caller_extension, "sub_extensions", app_off++))) {
						goto error;
					}

					if (!(x_caller_extension = switch_xml_add_child_d(x_inner_extension, "extension", i_off++))) {
						goto error;
					}
					switch_xml_set_attr_d(x_caller_extension, "name", cp->caller_extension->extension_name);
					switch_xml_set_attr_d(x_caller_extension, "number", cp->caller_extension->extension_number);
					switch_xml_set_attr_d(x_caller_extension, "dialplan", cp->dialplan);
					if (cp->caller_extension->current_application) {
						switch_xml_set_attr_d(x_caller_extension, "current_app", cp->caller_extension->current_application->application_name);
					}

					for (ap = cp->caller_extension->applications; ap; ap = ap->next) {
						if (!(x_application = switch_xml_add_child_d(x_caller_extension, "application", i_app_off++))) {
							goto error;
						}
						if (ap == cp->caller_extension->current_application) {
							switch_xml_set_attr_d(x_application, "last_executed", "true");
						}
						switch_xml_set_attr_d(x_application, "app_name", ap->application_name);
						switch_xml_set_attr_d(x_application, "app_data", ap->application_data);
					}
				}
			}
		}

		if (!(x_main_cp = switch_xml_add_child_d(x_callflow, "caller_profile", cf_off++))) {
			goto error;
		}

		cp_off += switch_ivr_set_xml_profile_data(x_main_cp, caller_profile, 0);

		if (caller_profile->originator_caller_profile) {
			switch_caller_profile_t *cp = NULL;
			int off = 0;
			if (!(x_o = switch_xml_add_child_d(x_main_cp, "originator", cp_off++))) {
				goto error;
			}

			for (cp = caller_profile->originator_caller_profile; cp; cp = cp->next) {
				if (!(x_caller_profile = switch_xml_add_child_d(x_o, "originator_caller_profile", off++))) {
					goto error;
				}
				switch_ivr_set_xml_profile_data(x_caller_profile, cp, 0);
			}
		}

		if (caller_profile->originatee_caller_profile) {
			switch_caller_profile_t *cp = NULL;
			int off = 0;
			if (!(x_o = switch_xml_add_child_d(x_main_cp, "originatee", cp_off++))) {
				goto error;
			}
			for (cp = caller_profile->originatee_caller_profile; cp; cp = cp->next) {
				if (!(x_caller_profile = switch_xml_add_child_d(x_o, "originatee_caller_profile", off++))) {
					goto error;
				}
				switch_ivr_set_xml_profile_data(x_caller_profile, cp, 0);
			}
		}

		if (caller_profile->times) {
			int t_off = 0;
			if (!(x_times = switch_xml_add_child_d(x_callflow, "times", cf_off++))) {
				goto error;
			}
			if (!(time_tag = switch_xml_add_child_d(x_times, "created_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->created);
			switch_xml_set_txt_d(time_tag, tmp);

			if (!(time_tag = switch_xml_add_child_d(x_times, "profile_created_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->profile_created);
			switch_xml_set_txt_d(time_tag, tmp);

			if (!(time_tag = switch_xml_add_child_d(x_times, "progress_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->progress);
			switch_xml_set_txt_d(time_tag, tmp);


			if (!(time_tag = switch_xml_add_child_d(x_times, "progress_media_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->progress_media);
			switch_xml_set_txt_d(time_tag, tmp);

			if (!(time_tag = switch_xml_add_child_d(x_times, "answered_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->answered);
			switch_xml_set_txt_d(time_tag, tmp);

			if (!(time_tag = switch_xml_add_child_d(x_times, "hangup_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->hungup);
			switch_xml_set_txt_d(time_tag, tmp);

			if (!(time_tag = switch_xml_add_child_d(x_times, "resurrect_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->resurrected);
			switch_xml_set_txt_d(time_tag, tmp);

			if (!(time_tag = switch_xml_add_child_d(x_times, "transfer_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->transferred);
			switch_xml_set_txt_d(time_tag, tmp);
		}

		caller_profile = caller_profile->next;
	}

	*xml_cdr = cdr;

	return SWITCH_STATUS_SUCCESS;

  error:

	if (cdr) {
		switch_xml_free(cdr);
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(void) switch_ivr_park_session(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_channel_set_state(channel, CS_PARK);
	switch_channel_set_flag(channel, CF_TRANSFER);

}

SWITCH_DECLARE(void) switch_ivr_delay_echo(switch_core_session_t *session, uint32_t delay_ms)
{
	stfu_instance_t *jb;
	int qlen = 0;
	stfu_frame_t *jb_frame;
	switch_frame_t *read_frame, write_frame = { 0 };
	switch_status_t status;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	uint32_t interval, samples;
	uint32_t ts = 0;
	switch_codec_implementation_t read_impl = { 0 };
	switch_core_session_get_read_impl(session, &read_impl);


	if (delay_ms < 1 || delay_ms > 10000) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid delay [%d] must be between 1 and 10000\n", delay_ms);
		return;
	}

	interval = read_impl.microseconds_per_packet / 1000;
	samples = switch_samples_per_packet(read_impl.samples_per_second, interval);

	qlen = delay_ms / (interval);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Setting delay to %dms (%d frames)\n", delay_ms, qlen);
	jb = stfu_n_init(qlen);

	write_frame.codec = switch_core_session_get_read_codec(session);

	while (switch_channel_ready(channel)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		stfu_n_eat(jb, ts, read_frame->payload, read_frame->data, read_frame->datalen);
		ts += interval;

		if ((jb_frame = stfu_n_read_a_frame(jb))) {
			write_frame.data = jb_frame->data;
			write_frame.datalen = (uint32_t) jb_frame->dlen;
			status = switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
			if (!SWITCH_READ_ACCEPTABLE(status)) {
				break;
			}
		}
	}

	stfu_n_destroy(&jb);
}

SWITCH_DECLARE(switch_status_t) switch_ivr_say(switch_core_session_t *session,
											   const char *tosay,
											   const char *module_name,
											   const char *say_type,
											   const char *say_method,
											   const char *say_gender,
											   switch_input_args_t *args)
{
	switch_say_interface_t *si;
	switch_channel_t *channel;
	switch_status_t status = SWITCH_STATUS_FALSE;
	const char *save_path = NULL, *chan_lang = NULL, *lang = NULL, *lname = NULL, *sound_path = NULL;
	switch_event_t *hint_data;
	switch_xml_t cfg, xml = NULL, language, macros;


	switch_assert(session);
	channel = switch_core_session_get_channel(session);
	switch_assert(channel);

	lang = switch_channel_get_variable(channel, "language");

	if (!lang) {
		chan_lang = switch_channel_get_variable(channel, "default_language");
		if (!chan_lang) {
			chan_lang = "en";
		}
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No language specified - Using [%s]\n", chan_lang);
	} else {
		chan_lang = lang;
	}

	switch_event_create(&hint_data, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(hint_data);

	switch_event_add_header_string(hint_data, SWITCH_STACK_BOTTOM, "macro_name", "say_app");
	switch_event_add_header_string(hint_data, SWITCH_STACK_BOTTOM, "lang", chan_lang);
	switch_channel_event_set_data(channel, hint_data);

	if (switch_xml_locate("phrases", NULL, NULL, NULL, &xml, &cfg, hint_data, SWITCH_TRUE) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Open of phrases failed.\n");
		goto done;
	}

	if (!(macros = switch_xml_child(cfg, "macros"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't find macros tag.\n");
		goto done;
	}

	if (!(language = switch_xml_child(macros, "language"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't find language tag.\n");
		goto done;
	}

	while (language) {
		if ((lname = (char *) switch_xml_attr(language, "name")) && !strcasecmp(lname, chan_lang)) {
			const char *tmp;

			if ((tmp = switch_xml_attr(language, "module"))) {
				module_name = tmp;
			}
			break;
		}
		language = language->next;
	}

	if (!language) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't find language %s.\n", chan_lang);
		goto done;
	}

	if (!(sound_path = (char *) switch_xml_attr(language, "sound-path"))) {
		sound_path = (char *) switch_xml_attr(language, "sound_path");
	}

	save_path = switch_channel_get_variable(channel, "sound_prefix");

	if (sound_path) {
		switch_channel_set_variable(channel, "sound_prefix", sound_path);
	}

	if ((si = switch_loadable_module_get_say_interface(module_name))) {
		/* should go back and proto all the say mods to const.... */
		switch_say_args_t say_args = {0};
		
		say_args.type = switch_ivr_get_say_type_by_name(say_type);
		say_args.method = switch_ivr_get_say_method_by_name(say_method);
		say_args.gender = switch_ivr_get_say_gender_by_name(say_gender);
		
		status = si->say_function(session, (char *) tosay, &say_args, args);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid SAY Interface [%s]!\n", module_name);
		status = SWITCH_STATUS_FALSE;
	}

  done:

	if (hint_data) {
		switch_event_destroy(&hint_data);
	}

	if (save_path) {
		switch_channel_set_variable(channel, "sound_prefix", save_path);
	}

	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

static const char *get_prefixed_str(char *buffer, size_t buffer_size, const char *prefix, size_t prefix_size, const char *str)
{
	size_t str_len;

	if (!buffer) {
		/*
		   if buffer is null then it just returns the str without the prefix appended, otherwise buffer contains the prefix followed by the original string
		 */

		return str;
	}

	str_len = strlen(str);
	memcpy(buffer, prefix, prefix_size);

	if (str_len + prefix_size + 1 > buffer_size) {
		memcpy(buffer + prefix_size, str, buffer_size - prefix_size - 1);
		buffer[buffer_size - prefix_size - 1] = '\0';
	} else {
		memcpy(buffer + prefix_size, str, str_len + 1);
	}

	return buffer;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_set_user(switch_core_session_t *session, const char *data)
{
	switch_xml_t x_domain, xml = NULL, x_user, x_param, x_params, x_group = NULL;
	char *user, *number_alias, *domain;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_FALSE;

	char *prefix_buffer = NULL, *prefix;
	size_t buffer_size = 0;
	size_t prefix_size = 0;
	if (zstr(data)) {
		goto error;
	}

	user = switch_core_session_strdup(session, data);

	if ((prefix = strchr(user, ' '))) {
		*prefix++ = 0;
	}

	if (!(domain = strchr(user, '@'))) {
		goto error;
	}

	*domain++ = '\0';

	if (switch_xml_locate_user("id", user, domain, NULL, &xml, &x_domain, &x_user, &x_group, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", user, domain);
		goto done;
	}

	status = SWITCH_STATUS_SUCCESS;

	if (!zstr(prefix)) {
		prefix_size = strlen(prefix);
		buffer_size = 1024 + prefix_size + 1;
		prefix_buffer = switch_core_session_alloc(session, buffer_size);
	}

	if ((number_alias = (char *) switch_xml_attr(x_user, "number-alias"))) {
		switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, "number_alias"), number_alias);
	}

	if ((x_params = switch_xml_child(x_domain, "variables"))) {
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (var && val) {
				switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, var), val);
			}
		}
	}

	if (x_group && (x_params = switch_xml_child(x_group, "variables"))) {
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (var && val) {
				switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, var), val);
			}
		}
	}

	if ((x_params = switch_xml_child(x_user, "variables"))) {
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (var && val) {
				switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, var), val);
			}
		}
	}

	switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, "user_name"), user);
	switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, "domain_name"), domain);

	goto done;

  error:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No user@domain specified.\n");

  done:
	if (xml) {
		switch_xml_free(xml);
	}

	return status;
}

SWITCH_DECLARE(switch_bool_t) switch_ivr_uuid_exists(const char *uuid)
{
	switch_bool_t exists = SWITCH_FALSE;
	switch_core_session_t *psession = NULL;

	if ((psession = switch_core_session_locate(uuid))) {
		switch_core_session_rwunlock(psession);
		exists = 1;
	}

	return exists;
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
