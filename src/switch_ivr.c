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
 * Anthony Minessale II <anthm@freeswitch.org>
 * Paul D. Tinsley <pdt at jackhammer.org>
 * Neal Horman <neal at wanlink dot com>
 * Matt Klein <mklein@nmedia.net>
 * Michael Jerris <mike@jerris.com>
 * Ken Rice <krice at suspicious dot org>
 * Marc Olivier Chouinard <mochouinard@moctel.com>
 *
 * switch_ivr.c -- IVR Library
 *
 */

#include <switch.h>
#include <switch_ivr.h>

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
	int32_t left;
	uint32_t elapsed;
	char data[2] = "";

	switch_frame_t write_frame = { 0 };
	unsigned char *abuf = NULL;
	switch_codec_implementation_t imp = { 0 };
	switch_codec_t codec = { 0 };
	int sval = 0;
	const char *var;

	arg_recursion_check_start(args);

	switch_core_session_get_read_impl(session, &imp);

	/*
	   if (switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_INBOUND && !switch_channel_test_flag(channel, CF_PROXY_MODE) && 
	   !switch_channel_media_ready(channel) && !switch_channel_test_flag(channel, CF_SERVICE)) {
	   if ((status = switch_channel_pre_answer(channel)) != SWITCH_STATUS_SUCCESS) {
	   switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot establish media.\n");
	   return SWITCH_STATUS_FALSE;
	   }
	   }
	 */

	if (!switch_channel_media_ready(channel)) {
		
		for (elapsed=0; switch_channel_up(channel) && elapsed<(ms/20); elapsed++) {
			if (switch_channel_test_flag(channel, CF_BREAK)) {
				switch_channel_clear_flag(channel, CF_BREAK);
				switch_goto_status(SWITCH_STATUS_BREAK, end);
			}
		
			switch_yield(20 * 1000);
		}
		switch_goto_status(SWITCH_STATUS_SUCCESS, end);
	}

	if ((var = switch_channel_get_variable(channel, SWITCH_SEND_SILENCE_WHEN_IDLE_VARIABLE))
		&& (sval = atoi(var))) {
		SWITCH_IVR_VERIFY_SILENCE_DIVISOR(sval);
	}

	if (ms > 10 && sval) {

		if (switch_core_codec_init(&codec,
								   "L16",
								   NULL,
								   imp.actual_samples_per_second,
								   imp.microseconds_per_packet / 1000,
								   imp.number_of_channels,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
								   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Error L16@%uhz %u channels %dms\n",
							  imp.actual_samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);
			switch_goto_status(SWITCH_STATUS_FALSE, end);
		}


		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Codec Activated L16@%uhz %u channels %dms\n",
						  imp.actual_samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);

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
		switch_goto_status(SWITCH_STATUS_SUCCESS, end);
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


		if (args) {
			switch_dtmf_t dtmf = {0};

			/*
			   dtmf handler function you can hook up to be executed when a digit is dialed during playback 
			   if you return anything but SWITCH_STATUS_SUCCESS the playback will stop.
			 */
			if (switch_channel_has_dtmf(channel)) {
				if (!args->input_callback && !args->buf && !args->dmachine) {
					status = SWITCH_STATUS_BREAK;
					break;
				}
				switch_channel_dequeue_dtmf(channel, &dtmf);

				if (args->dmachine) {
					char ds[2] = {dtmf.digit, '\0'};
					if ((status = switch_ivr_dmachine_feed(args->dmachine, ds, NULL)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				} 

				if (args->input_callback) {
					status = args->input_callback(session, (void *) &dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen);
				} else if (args->buf) {
					*((char *) args->buf) = dtmf.digit;
					status = SWITCH_STATUS_BREAK;
				}
			}

			if (args->input_callback) {
				switch_event_t *event = NULL;

				if (switch_core_session_dequeue_event(session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
					switch_status_t ostatus = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);
					if (ostatus != SWITCH_STATUS_SUCCESS) {
						status = ostatus;
					}
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

		if (args && args->dmachine) {
			if ((status = switch_ivr_dmachine_ping(args->dmachine, NULL)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}

		if (sval && write_frame.datalen) {
			switch_generate_sln_silence((int16_t *) write_frame.data, write_frame.samples, imp.number_of_channels, sval);
			switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
		} else {
			switch_core_session_write_frame(session, &cng_frame, SWITCH_IO_FLAG_NONE, 0);
		}
	}


 end:

	arg_recursion_check_stop(args);

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

	if (!conninfo) {
		return NULL;
	}

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
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, switch_core_session_get_pool(conninfo->session));
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_set_flag_locked(conninfo, SUF_THREAD_RUNNING);
	switch_thread_create(&conninfo->thread, thd_attr, unicast_thread_run, conninfo, switch_core_session_get_pool(conninfo->session));
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
		switch_status_t st;

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Shutting down unicast connection\n");
		switch_clear_flag_locked(conninfo, SUF_READY);
		switch_socket_shutdown(conninfo->socket, SWITCH_SHUTDOWN_READWRITE);
		switch_thread_join(&st, conninfo->thread);
		
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
	unsigned long CMD_XFEREXT = switch_hashfunc_default("xferext", &hlen);
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
		char *event_uuid = switch_event_get_header(event, "event-uuid");
		char *app_arg = switch_event_get_header(event, "execute-app-arg");
		char *content_type = switch_event_get_header(event, "content-type");
		char *loop_h = switch_event_get_header(event, "loops");
		char *hold_bleg = switch_event_get_header(event, "hold-bleg");
		int loops = 1;
		int inner = 0;

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

			if (!switch_channel_test_flag(channel, CF_BRIDGED) || switch_channel_test_flag(channel, CF_BROADCAST)) {
				inner++;
				hold_bleg = NULL;
			} 

			if (!switch_channel_test_flag(channel, CF_BROADCAST)) {
				switch_channel_set_flag(channel, CF_BROADCAST);
				if (inner) {
					inner--;
				}
			}

			if (hold_bleg && switch_true(hold_bleg)) {
				if ((b_uuid = switch_channel_get_partner_uuid(channel))) {
					const char *stream;
					b_uuid = switch_core_session_strdup(session, b_uuid);

					if (!(stream = switch_channel_get_hold_music_partner(channel))) {
						stream = switch_channel_get_hold_music(channel);
					}

					if (stream && switch_is_moh(stream)) {
						if ((b_session = switch_core_session_locate(b_uuid))) {
							switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
							switch_status_t st;
							
							switch_ivr_broadcast(b_uuid, stream, SMF_ECHO_ALEG | SMF_LOOP);
							st = switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_TRUE, 5000, NULL);
							if (st != SWITCH_STATUS_SUCCESS && 
								switch_channel_ready(channel) && switch_channel_ready(b_channel) && !switch_channel_test_flag(b_channel, CF_BROADCAST)) {
								switch_core_session_kill_channel(b_session, SWITCH_SIG_BREAK);
								st = switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_TRUE, 5000, NULL);
							
								if (st != SWITCH_STATUS_SUCCESS && 
									switch_channel_ready(channel) && switch_channel_ready(b_channel) && !switch_channel_test_flag(b_channel, CF_BROADCAST)) {
									switch_core_session_flush_private_events(b_session);
								}
							}
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

				if (event_uuid) {
					switch_channel_set_variable(channel, "app_uuid", event_uuid);
				}

				switch_channel_set_variable_printf(channel, "current_loop", "%d", x + 1);
				switch_channel_set_variable_printf(channel, "total_loops", "%d", loops);

				if (switch_core_session_execute_application(session, app_name, app_arg) != SWITCH_STATUS_SUCCESS) {
					if (!inner || switch_channel_test_flag(channel, CF_STOP_BROADCAST)) switch_channel_clear_flag(channel, CF_BROADCAST);
					break;
				}

				aftr = switch_micro_time_now();
				if (!switch_channel_ready(channel) || switch_channel_test_flag(channel, CF_STOP_BROADCAST) || aftr - b4 < 500000) {
					break;
				}
			}

			switch_channel_set_variable(channel, "current_loop", NULL);
			switch_channel_set_variable(channel, "total_loops", NULL);

			if (b_uuid) {
				if ((b_session = switch_core_session_locate(b_uuid))) {
					switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
					switch_channel_stop_broadcast(b_channel);
					switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
					switch_core_session_rwunlock(b_session);
				}
			}

			if (!inner) {
				switch_channel_clear_flag(channel, CF_BROADCAST);
			}

			if (switch_channel_test_flag(channel, CF_STOP_BROADCAST)) {
				switch_channel_clear_flag(channel, CF_BROADCAST);
				switch_channel_set_flag(channel, CF_BREAK); 
			}
			
			switch_channel_audio_sync(channel);
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

	} else if (cmd_hash == CMD_XFEREXT) {
		switch_event_header_t *hp;
		switch_caller_extension_t *extension = NULL;


		if ((extension = switch_caller_extension_new(session, "xferext", "xferext")) == 0) {
			abort();
		}
		
		for (hp = event->headers; hp; hp = hp->next) {
			char *app;
			char *data;
			
			if (!strcasecmp(hp->name, "application")) {
				app = strdup(hp->value);
				if (app) {
					data = strchr(app, ' ');

					if (data) {
						*data++ = '\0';
					}

					switch_caller_extension_add_application(session, extension, app, data);
					free(app);
				}
			}
		}

		switch_channel_transfer_to_extension(channel, extension);
		
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
		event->event_id = SWITCH_EVENT_PRIVATE_COMMAND;
		switch_event_prep_for_delivery(event);
		switch_channel_event_set_data(switch_core_session_get_channel(session), event);
		switch_event_fire(&event);
	}

	return status;

}

SWITCH_DECLARE(switch_status_t) switch_ivr_process_indications(switch_core_session_t *session, switch_core_session_message_t *message)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *channel = switch_core_session_get_channel(session);
		
		switch(message->message_id) {
		case SWITCH_MESSAGE_INDICATE_ANSWER:
			if (switch_channel_answer(channel) != SWITCH_STATUS_SUCCESS) {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
			break;
		case SWITCH_MESSAGE_INDICATE_PROGRESS:
			if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
			break;
		case SWITCH_MESSAGE_INDICATE_RINGING:
			if (switch_channel_ring_ready(channel) != SWITCH_STATUS_SUCCESS) {
				switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			}
			break;
		default:
		status = SWITCH_STATUS_FALSE;
			break;
		}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_parse_all_messages(switch_core_session_t *session)
{
	switch_core_session_message_t *message;
	int i = 0;

	switch_ivr_parse_all_signal_data(session);

	while (switch_core_session_dequeue_message(session, &message) == SWITCH_STATUS_SUCCESS) {
		i++;

		if (switch_ivr_process_indications(session, message) == SWITCH_STATUS_SUCCESS) {
			switch_core_session_free_message(&message);
		} else {
			switch_core_session_receive_message(session, message);
			message = NULL;
		}
	}

	return i ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_parse_all_signal_data(switch_core_session_t *session)
{
	void *data;
	switch_core_session_message_t msg = { 0 };
	int i = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!switch_core_session_in_thread(session)) {
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_test_flag(channel, CF_SIGNAL_DATA)) {
		return SWITCH_STATUS_FALSE;
	}

	switch_channel_set_flag(channel, CF_SIGNAL_DATA);

	msg.message_id = SWITCH_MESSAGE_INDICATE_SIGNAL_DATA;
	msg.from = __FILE__;

	while (switch_core_session_dequeue_signal_data(session, &data) == SWITCH_STATUS_SUCCESS) {
		i++;
	
		msg.pointer_arg = data;	
		switch_core_session_receive_message(session, &msg);

		data = NULL;

	}

	switch_channel_clear_flag(channel, CF_SIGNAL_DATA);

	return i ? SWITCH_STATUS_SUCCESS : SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_parse_all_events(switch_core_session_t *session)
{
	int x = 0;
	switch_channel_t *channel;

	switch_ivr_parse_all_messages(session);

	channel = switch_core_session_get_channel(session);

	if (!switch_channel_test_flag(channel, CF_PROXY_MODE) && switch_channel_test_flag(channel, CF_BLOCK_BROADCAST_UNTIL_MEDIA)) {
		if (switch_channel_media_up(channel)) {
			switch_channel_clear_flag(channel, CF_BLOCK_BROADCAST_UNTIL_MEDIA);
		} else {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	while (switch_ivr_parse_next_event(session) == SWITCH_STATUS_SUCCESS) {
		x++;
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
	switch_codec_t codec = { 0 };
	int sval = 0;
	const char *var;
	switch_frame_t write_frame = { 0 };
	unsigned char *abuf = NULL;
	switch_codec_implementation_t imp = { 0 };



	if (switch_channel_test_flag(channel, CF_RECOVERED) && switch_channel_test_flag(channel, CF_CONTROLLED)) {
		switch_channel_clear_flag(channel, CF_CONTROLLED);
	}

	if (switch_channel_test_flag(channel, CF_CONTROLLED)) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Cannot park channels that are under control already.\n");
		return SWITCH_STATUS_FALSE;
	}

	if (switch_channel_get_state(channel) == CS_RESET) {
		return SWITCH_STATUS_FALSE;
	}

	arg_recursion_check_start(args);

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

			if ((var = switch_channel_get_variable(channel, SWITCH_SEND_SILENCE_WHEN_IDLE_VARIABLE)) && (sval = atoi(var))) {
				switch_core_session_get_read_impl(session, &imp);

				if (switch_core_codec_init(&codec,
								   "L16",
								   NULL,
								   imp.actual_samples_per_second,
								   imp.microseconds_per_packet / 1000,
								   imp.number_of_channels,
								   SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
								   switch_core_session_get_pool(session)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Codec Error L16@%uhz %u channels %dms\n",
									  imp.samples_per_second, imp.number_of_channels, imp.microseconds_per_packet / 1000);
					switch_goto_status(SWITCH_STATUS_FALSE, end);
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
		}

		if (rate) {
			if (switch_channel_test_flag(channel, CF_SERVICE)) {
				switch_cond_next();
				status = SWITCH_STATUS_SUCCESS;
			} else {
				status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, stream_id);
			}
		} else {
			switch_yield(20000);

			if (switch_core_session_dequeue_private_event(session, &event) == SWITCH_STATUS_SUCCESS) {
				switch_ivr_parse_event(session, event);
				switch_event_destroy(&event);
			}

			status = SWITCH_STATUS_SUCCESS;
		}

		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (rate && write_frame.data && sval) {
			switch_generate_sln_silence((int16_t *) write_frame.data, write_frame.samples, read_impl.number_of_channels, sval);
			switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
		}
		
		if (expires && switch_epoch_time_now(NULL) >= expires) {
			switch_channel_hangup(channel, timeout_cause);
			break;
		}

		if (switch_channel_test_flag(channel, CF_UNICAST)) {
			if (!switch_channel_media_ready(channel)) {
				if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
					switch_goto_status(SWITCH_STATUS_FALSE, end);
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
				
			if (args && !args->input_callback && !args->buf && !args->dmachine) {
				status = SWITCH_STATUS_BREAK;
				break;
			}
				
			switch_channel_dequeue_dtmf(channel, &dtmf);

			if (args) {
				if (args->dmachine) {
					char ds[2] = {dtmf.digit, '\0'};
					if ((status = switch_ivr_dmachine_feed(args->dmachine, ds, NULL)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				} 

				if (args->input_callback) {
					if ((status = args->input_callback(session, (void *) &dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen)) != SWITCH_STATUS_SUCCESS) {
						break;
					}
				}
			}
		}

		if (switch_core_session_dequeue_event(session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			if (args && args->input_callback) {
				switch_status_t ostatus;

				if ((ostatus = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen)) != SWITCH_STATUS_SUCCESS) {
					status = ostatus;
					break;
				}
			} else {
				switch_channel_event_set_data(channel, event);
				switch_event_fire(&event);
			}
		}
			
		if (args && args->dmachine) {
			if ((status = switch_ivr_dmachine_ping(args->dmachine, NULL)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
		

	}

 end:

	arg_recursion_check_stop(args);

	if (write_frame.codec) {
		switch_core_codec_destroy(&codec);
	}

	switch_safe_free(abuf);

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

	if (!args) {
		return SWITCH_STATUS_GENERR;
	}

	arg_recursion_check_start(args);

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
			if (!args->input_callback && !args->buf && !args->dmachine) {
				status = SWITCH_STATUS_BREAK;
				break;
			}
			switch_channel_dequeue_dtmf(channel, &dtmf);

			if (args->dmachine) {
				char ds[2] = {dtmf.digit, '\0'};
				if ((status = switch_ivr_dmachine_feed(args->dmachine, ds, NULL)) != SWITCH_STATUS_SUCCESS) {
					break;
				}
			} 

			if (args->input_callback) {
				status = args->input_callback(session, (void *) &dtmf, SWITCH_INPUT_TYPE_DTMF, args->buf, args->buflen);
			}

			if (digit_timeout) {
				digit_started = switch_micro_time_now();
			}
		}

		if (switch_core_session_dequeue_event(session, &event, SWITCH_FALSE) == SWITCH_STATUS_SUCCESS) {
			switch_status_t ostatus = args->input_callback(session, event, SWITCH_INPUT_TYPE_EVENT, args->buf, args->buflen);
			if (ostatus != SWITCH_STATUS_SUCCESS) {
				status = ostatus;
			}
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

		if (args && args->dmachine) {
			if ((status = switch_ivr_dmachine_ping(args->dmachine, NULL)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}

		if (read_frame && args && (args->read_frame_callback)) {
			if ((status = args->read_frame_callback(session, read_frame, args->user_data)) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		}
	}

	arg_recursion_check_stop(args);

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
				switch_safe_free(abuf);
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
					switch_safe_free(abuf);
					return SWITCH_STATUS_SUCCESS;
				}


				buf[x++] = dtmf.digit;
				buf[x] = '\0';

				if (x >= buflen || x >= maxdigits) {
					switch_safe_free(abuf);
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
				switch_generate_sln_silence((int16_t *) write_frame.data, write_frame.samples, imp.number_of_channels, sval);
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
	switch_event_t *event;

	msg.message_id = SWITCH_MESSAGE_INDICATE_HOLD;
	msg.string_arg = message;
	msg.from = __FILE__;

	switch_channel_set_flag(channel, CF_HOLD);
	switch_channel_set_flag(channel, CF_SUSPEND);

	switch_core_session_receive_message(session, &msg);

	if (moh && (stream = switch_channel_get_hold_music(channel))) {
		if ((other_uuid = switch_channel_get_partner_uuid(channel))) {
			switch_ivr_broadcast(other_uuid, stream, SMF_ECHO_ALEG | SMF_LOOP);
		}
	}

	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_HOLD) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
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

SWITCH_DECLARE(switch_status_t) switch_ivr_hold_toggle_uuid(const char *uuid, const char *message, switch_bool_t moh)
{
	switch_core_session_t *session;
	switch_channel_t *channel;
	switch_channel_callstate_t callstate;

	if ((session = switch_core_session_locate(uuid))) {
		if ((channel = switch_core_session_get_channel(session))) {
			callstate = switch_channel_get_callstate(channel);

			if (callstate == CCS_ACTIVE) {
				switch_ivr_hold(session, message, moh);
			} else if (callstate == CCS_HELD) {
				switch_ivr_unhold(session);
			}
		}
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
	switch_event_t *event;

	msg.message_id = SWITCH_MESSAGE_INDICATE_UNHOLD;
	msg.from = __FILE__;

	switch_channel_clear_flag(channel, CF_HOLD);
	switch_channel_clear_flag(channel, CF_SUSPEND);

	switch_core_session_receive_message(session, &msg);


	if ((other_uuid = switch_channel_get_partner_uuid(channel)) && (b_session = switch_core_session_locate(other_uuid))) {
		switch_channel_t *b_channel = switch_core_session_get_channel(b_session);
		switch_channel_stop_broadcast(b_channel);
		switch_channel_wait_for_flag(b_channel, CF_BROADCAST, SWITCH_FALSE, 5000, NULL);
		switch_core_session_rwunlock(b_session);
	}


	if (switch_event_create(&event, SWITCH_EVENT_CHANNEL_UNHOLD) == SWITCH_STATUS_SUCCESS) {
		switch_channel_event_set_data(channel, event);
		switch_event_fire(&event);
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
		
		if (switch_channel_test_flag(channel, CF_MEDIA_TRANS)) {
			switch_core_session_rwunlock(session);
			return SWITCH_STATUS_INUSE;
		}

		switch_channel_set_flag(channel, CF_MEDIA_TRANS);

		if ((flags & SMF_REBRIDGE) && !switch_channel_test_flag(channel, CF_BRIDGE_ORIGINATOR)) {
			swap = 1;
		}

		if (switch_channel_test_flag(channel, CF_PROXY_MODE)) {
			status = SWITCH_STATUS_SUCCESS;

			/* If we had early media in bypass mode before, it is no longer relevant */
			if (switch_channel_test_flag(channel, CF_EARLY_MEDIA)) {
				switch_core_session_message_t msg2 = { 0 };
				
				msg2.message_id = SWITCH_MESSAGE_INDICATE_CLEAR_PROGRESS;
				msg2.from = __FILE__;
				switch_core_session_receive_message(session, &msg2);
			}
			
			if ((flags & SMF_REPLYONLY_A)) {
				msg.numeric_arg = 1;
			}
					
			if (switch_core_session_receive_message(session, &msg) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Can't re-establsh media on %s\n", switch_channel_get_name(channel));
				switch_core_session_rwunlock(session);
				return SWITCH_STATUS_GENERR;
			}

			if ((flags & SMF_REPLYONLY_B)) {
				msg.numeric_arg = 1;
			} else {
				msg.numeric_arg = 0;
			}

			if ((flags & SMF_IMMEDIATE)) {
				switch_channel_wait_for_flag(channel, CF_REQ_MEDIA, SWITCH_FALSE, 250, NULL);
				switch_yield(250000);
			} else {
				switch_channel_wait_for_flag(channel, CF_REQ_MEDIA, SWITCH_FALSE, 10000, NULL);
				switch_channel_wait_for_flag(channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);
				switch_channel_wait_for_flag(channel, CF_MEDIA_SET, SWITCH_TRUE, 10000, NULL);
				switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
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

		switch_channel_clear_flag(channel, CF_MEDIA_TRANS);
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

		if (switch_channel_test_flag(channel, CF_SECURE)) {
			switch_core_session_rwunlock(session);
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, 
							  "Cannot bypass %s due to secure connection.\n", switch_channel_get_name(channel));
			return SWITCH_STATUS_FALSE;
		}

		if (switch_channel_test_flag(channel, CF_MEDIA_TRANS)) {
			switch_core_session_rwunlock(session);
			return SWITCH_STATUS_INUSE;
		}

		switch_channel_set_flag(channel, CF_MEDIA_TRANS);

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

				if (!switch_core_session_in_thread(session)) {
					switch_channel_set_state(channel, CS_PARK);
				}
				switch_channel_set_state(other_channel, CS_PARK);
				if (switch_core_session_in_thread(session)) {
					switch_yield(100000);
				} else {
					switch_channel_wait_for_state(other_channel, channel, CS_PARK);
				}
				switch_core_session_receive_message(other_session, &msg);
				switch_channel_wait_for_flag(other_channel, CF_REQ_MEDIA, SWITCH_FALSE, 10000, NULL);
				//switch_channel_wait_for_flag(other_channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);
				switch_channel_wait_for_flag(other_channel, CF_MEDIA_SET, SWITCH_TRUE, 10000, NULL);
			}

			switch_core_session_receive_message(session, &msg);

			if (other_channel) {
				if (!switch_core_session_in_thread(session)) {
					switch_channel_wait_for_state(channel, NULL, CS_PARK);
					switch_channel_wait_for_flag(channel, CF_REQ_MEDIA, SWITCH_FALSE, 10000, NULL);
					switch_channel_wait_for_flag(channel, CF_MEDIA_ACK, SWITCH_TRUE, 10000, NULL);
					switch_channel_wait_for_flag(channel, CF_MEDIA_SET, SWITCH_TRUE, 10000, NULL);
				}

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

		switch_channel_clear_flag(channel, CF_MEDIA_TRANS);
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
	const char *use_dialplan = dialplan, *use_context = context;

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

	/* reset temp hold music */
	switch_channel_set_variable(channel, SWITCH_TEMP_HOLD_MUSIC_VARIABLE, NULL);

	if ((profile = switch_channel_get_caller_profile(channel))) {
		const char *var;

		if (zstr(dialplan) && (var = switch_channel_get_variable(channel, "force_transfer_dialplan"))) {
			use_dialplan = var;
		}

		if (zstr(context) && (var = switch_channel_get_variable(channel, "force_transfer_context"))) {
			use_context = var;
		}

		if (zstr(use_dialplan)) {
			use_dialplan = profile->dialplan;
			if (!zstr(use_dialplan) && !strcasecmp(use_dialplan, "inline")) {
				use_dialplan = NULL;
			}
		}

		if (zstr(use_context)) {
			use_context = profile->context;
		}

		if (zstr(use_dialplan)) {
			use_dialplan = "XML";
		}

		if (zstr(use_context)) {
			use_context = "default";
		}

		if (zstr(extension)) {
			extension = "service";
		}

		new_profile = switch_caller_profile_clone(session, profile);

		new_profile->dialplan = switch_core_strdup(new_profile->pool, use_dialplan);
		new_profile->context = switch_core_strdup(new_profile->pool, use_context);
		new_profile->destination_number = switch_core_strdup(new_profile->pool, extension);
		new_profile->rdnis = switch_core_strdup(new_profile->pool, profile->destination_number);

		switch_channel_set_variable(channel, SWITCH_SIGNAL_BOND_VARIABLE, NULL);

		/* Set CF_TRANSFER flag before hanging up bleg to avoid race condition */
		switch_channel_set_flag(channel, CF_TRANSFER);

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

		switch_channel_set_state(channel, CS_ROUTING);
		switch_channel_audio_sync(channel);

		msg.message_id = SWITCH_MESSAGE_INDICATE_TRANSFER;
		msg.from = __FILE__;
		switch_core_session_receive_message(session, &msg);

		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_NOTICE, "Transfer %s to %s[%s@%s]\n", switch_channel_get_name(channel), use_dialplan,
						  extension, use_context);


		new_profile->transfer_source = switch_core_sprintf(new_profile->pool, "%ld:%s:bl_xfer:%s/%s/%s", 
														   (long) switch_epoch_time_now(NULL), new_profile->uuid_str,
														   extension, use_context, use_dialplan);
		switch_channel_add_variable_var_check(channel, SWITCH_TRANSFER_HISTORY_VARIABLE, new_profile->transfer_source, SWITCH_FALSE, SWITCH_STACK_PUSH);
		switch_channel_set_variable_var_check(channel, SWITCH_TRANSFER_SOURCE_VARIABLE, new_profile->transfer_source, SWITCH_FALSE);
		return SWITCH_STATUS_SUCCESS;
	}

	return SWITCH_STATUS_FALSE;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_transfer_variable(switch_core_session_t *sessa, switch_core_session_t *sessb, char *var)
{
	switch_channel_t *chana = switch_core_session_get_channel(sessa);
	switch_channel_t *chanb = switch_core_session_get_channel(sessb);
	switch_event_t *var_event;

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

		switch_channel_get_variables(chana, &var_event);

		for (hi = var_event->headers; hi; hi = hi->next) {
			char *vvar = hi->name;
			char *vval = hi->value;
			if (vvar && vval && (!prefix || (var && !strncmp((char *) vvar, var, strlen(var))))) {
				switch_channel_set_variable(chanb, (char *) vvar, (char *) vval);
			}
		}

		switch_event_destroy(&var_event);
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
			switch_core_hash_init(&(*parser)->hash);

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

	if (!(param = switch_xml_add_child_d(xml, "caller_id_number", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->caller_id_number);

	if (!(param = switch_xml_add_child_d(xml, "callee_id_name", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->callee_id_name);

	if (!(param = switch_xml_add_child_d(xml, "callee_id_number", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->callee_id_number);

	if (!(param = switch_xml_add_child_d(xml, "ani", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->ani);

	if (!(param = switch_xml_add_child_d(xml, "aniii", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->aniii);


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

	if (caller_profile->transfer_source) {
		if (!(param = switch_xml_add_child_d(xml, "transfer_source", off++))) {
			return -1;
		}
		switch_xml_set_txt_d(param, caller_profile->transfer_source);
	}

	if (!(param = switch_xml_add_child_d(xml, "context", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->context);

	if (!(param = switch_xml_add_child_d(xml, "chan_name", off++))) {
		return -1;
	}
	switch_xml_set_txt_d(param, caller_profile->chan_name);


	if (caller_profile->soft) {
		profile_node_t *pn;

		for (pn = caller_profile->soft; pn; pn = pn->next) {

			if (!(param = switch_xml_add_child_d(xml, pn->var, off++))) {
				return -1;
			}
			switch_xml_set_txt_d(param, pn->val);
		}

	}


	return off;
}

static int switch_ivr_set_xml_chan_var(switch_xml_t xml, const char *var, const char *val, int off)
{
	char *data;
	switch_size_t dlen = strlen(val) * 3 + 1;
	switch_xml_t variable;

	if (!val) val = "";
	
	if (!zstr(var) && ((variable = switch_xml_add_child_d(xml, var, off++)))) {
		if ((data = malloc(dlen))) {
			memset(data, 0, dlen);
			switch_url_encode(val, data, dlen);
			switch_xml_set_txt_d(variable, data);
			free(data);
		} else abort();
	}
	
	return off;
	
}


SWITCH_DECLARE(int) switch_ivr_set_xml_chan_vars(switch_xml_t xml, switch_channel_t *channel, int off)
{

	switch_event_header_t *hi = switch_channel_variable_first(channel);

	if (!hi)
		return off;

	for (; hi; hi = hi->next) {
		if (hi->idx) {
			int i;
			
			for (i = 0; i < hi->idx; i++) {
				off = switch_ivr_set_xml_chan_var(xml, hi->name, hi->array[i], off);
			}
		} else {
			off = switch_ivr_set_xml_chan_var(xml, hi->name, hi->value, off);
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
		x_application, x_callflow, x_inner_extension, x_apps, x_o, x_channel_data, x_field, xhr, x_hold;
	switch_app_log_t *app_log;
	char tmp[512], *f;
	int cdr_off = 0, v_off = 0, cd_off = 0;
	switch_hold_record_t *hold_record = switch_channel_get_hold_record(channel), *hr;
	
	if (*xml_cdr) {
		cdr = *xml_cdr;
	} else {
		if (!(cdr = switch_xml_new("cdr"))) {
			return SWITCH_STATUS_SUCCESS;
		}
	}

	switch_xml_set_attr_d(cdr, "core-uuid", switch_core_get_uuid());
	switch_xml_set_attr_d(cdr, "switchname", switch_core_get_switchname());

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
			char tmp[128];

			if (!(x_application = switch_xml_add_child_d(x_apps, "application", app_off++))) {
				goto error;
			}

			switch_xml_set_attr_d(x_application, "app_name", ap->app);
			switch_xml_set_attr_d(x_application, "app_data", ap->arg);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, ap->stamp);
			switch_xml_set_attr_d_buf(x_application, "app_stamp", tmp);
		}
	}

	if (hold_record) {
		int cf_off = 0;

		if (!(xhr = switch_xml_add_child_d(cdr, "hold-record", cdr_off++))) {
			goto error;
		}

		for (hr = hold_record; hr; hr = hr->next) {
			char *t = tmp;
			if (!(x_hold = switch_xml_add_child_d(xhr, "hold", cf_off++))) {
				goto error;
			}

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, hr->on);
			switch_xml_set_attr_d(x_hold, "on", t);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, hr->off);
			switch_xml_set_attr_d(x_hold, "off", t);

			if (hr->uuid) {
				switch_xml_set_attr_d(x_hold, "bridged-to", hr->uuid);
			}


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

		if (!zstr(caller_profile->uuid_str)) {
			switch_xml_set_attr_d(x_callflow, "unique-id", caller_profile->uuid_str);
		}

		if (!zstr(caller_profile->clone_of)) {
			switch_xml_set_attr_d(x_callflow, "clone-of", caller_profile->clone_of);
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

		if (caller_profile->origination_caller_profile) {
			switch_caller_profile_t *cp = NULL;
			int off = 0;
			if (!(x_o = switch_xml_add_child_d(x_main_cp, "origination", cp_off++))) {
				goto error;
			}

			for (cp = caller_profile->origination_caller_profile; cp; cp = cp->next) {
				if (!(x_caller_profile = switch_xml_add_child_d(x_o, "origination_caller_profile", off++))) {
					goto error;
				}
				switch_ivr_set_xml_profile_data(x_caller_profile, cp, 0);
			}
		}

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

			if (!(time_tag = switch_xml_add_child_d(x_times, "bridged_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->bridged);
			switch_xml_set_txt_d(time_tag, tmp);

			if (!(time_tag = switch_xml_add_child_d(x_times, "last_hold_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->last_hold);
			switch_xml_set_txt_d(time_tag, tmp);
			
			if (!(time_tag = switch_xml_add_child_d(x_times, "hold_accum_time", t_off++))) {
				goto error;
			}
			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->hold_accum);
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

static void switch_ivr_set_json_profile_data(cJSON *json, switch_caller_profile_t *caller_profile)
{
	cJSON_AddItemToObject(json, "username", cJSON_CreateString((char *)caller_profile->username));
	cJSON_AddItemToObject(json, "dialplan", cJSON_CreateString((char *)caller_profile->dialplan));
	cJSON_AddItemToObject(json, "caller_id_name", cJSON_CreateString((char *)caller_profile->caller_id_name));
	cJSON_AddItemToObject(json, "ani", cJSON_CreateString((char *)caller_profile->ani));
	cJSON_AddItemToObject(json, "aniii", cJSON_CreateString((char *)caller_profile->aniii));
	cJSON_AddItemToObject(json, "caller_id_number", cJSON_CreateString((char *)caller_profile->caller_id_number));
	cJSON_AddItemToObject(json, "network_addr", cJSON_CreateString((char *)caller_profile->network_addr));
	cJSON_AddItemToObject(json, "rdnis", cJSON_CreateString((char *)caller_profile->rdnis));
	cJSON_AddItemToObject(json, "destination_number", cJSON_CreateString(caller_profile->destination_number));
	cJSON_AddItemToObject(json, "uuid", cJSON_CreateString(caller_profile->uuid));
	cJSON_AddItemToObject(json, "source", cJSON_CreateString((char *)caller_profile->source));
	cJSON_AddItemToObject(json, "context", cJSON_CreateString((char *)caller_profile->context));
	cJSON_AddItemToObject(json, "chan_name", cJSON_CreateString(caller_profile->chan_name));
}

static void switch_ivr_set_json_chan_vars(cJSON *json, switch_channel_t *channel, switch_bool_t urlencode)
{
	switch_event_header_t *hi = switch_channel_variable_first(channel);

	if (!hi)
		return;

	for (; hi; hi = hi->next) {
		if (!zstr(hi->name) && !zstr(hi->value)) {
			char *data = hi->value;
			if (urlencode) {
				switch_size_t dlen = strlen(hi->value) * 3;

				if ((data = malloc(dlen))) {
					memset(data, 0, dlen);
					switch_url_encode(hi->value, data, dlen);
				}
			}

			cJSON_AddItemToObject(json, hi->name, cJSON_CreateString(data));

			if (data != hi->value) {
				switch_safe_free(data);
			}
		}
	}
	switch_channel_variable_last(channel);
}



SWITCH_DECLARE(switch_status_t) switch_ivr_generate_json_cdr(switch_core_session_t *session, cJSON **json_cdr, switch_bool_t urlencode)
{
	cJSON *cdr = cJSON_CreateObject();
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_caller_profile_t *caller_profile;
	cJSON *variables, *j_main_cp, *j_caller_profile, *j_caller_extension, *j_caller_extension_apps, *j_times,
		*j_application, *j_callflow, *j_inner_extension, *j_app_log, *j_apps, *j_o, *j_o_profiles, *j_channel_data;
	switch_app_log_t *app_log;
	char tmp[512], *f;

	cJSON_AddItemToObject(cdr, "core-uuid", cJSON_CreateString(switch_core_get_uuid()));
	cJSON_AddItemToObject(cdr, "switchname", cJSON_CreateString(switch_core_get_switchname()));
	j_channel_data = cJSON_CreateObject();

	cJSON_AddItemToObject(cdr, "channel_data", j_channel_data);

	cJSON_AddItemToObject(j_channel_data, "state", cJSON_CreateString((char *) switch_channel_state_name(switch_channel_get_state(channel))));
	cJSON_AddItemToObject(j_channel_data, "direction", cJSON_CreateString(switch_channel_direction(channel) == SWITCH_CALL_DIRECTION_OUTBOUND ? "outbound" : "inbound"));

	switch_snprintf(tmp, sizeof(tmp), "%d", switch_channel_get_state(channel));
	cJSON_AddItemToObject(j_channel_data, "state_number", cJSON_CreateString((char *) tmp));
	
	if ((f = switch_channel_get_flag_string(channel))) {
		cJSON_AddItemToObject(j_channel_data, "flags", cJSON_CreateString((char *) f));
		free(f);
	}

	if ((f = switch_channel_get_cap_string(channel))) {
		cJSON_AddItemToObject(j_channel_data, "caps", cJSON_CreateString((char *) f));
		free(f);
	}

	variables = cJSON_CreateObject();
	cJSON_AddItemToObject(cdr, "variables", variables);

	switch_ivr_set_json_chan_vars(variables, channel, urlencode);


	if ((app_log = switch_core_session_get_app_log(session))) {
		switch_app_log_t *ap;

		j_app_log = cJSON_CreateObject();
		j_apps = cJSON_CreateArray();

		cJSON_AddItemToObject(cdr, "app_log", j_app_log);
		cJSON_AddItemToObject(j_app_log, "applications", j_apps);

		for (ap = app_log; ap; ap = ap->next) {
			j_application = cJSON_CreateObject();

			cJSON_AddItemToObject(j_application, "app_name", cJSON_CreateString(ap->app));
			cJSON_AddItemToObject(j_application, "app_data", cJSON_CreateString(ap->arg));

			cJSON_AddItemToArray(j_apps, j_application);
		}
	}


	caller_profile = switch_channel_get_caller_profile(channel);

	while (caller_profile) {

		j_callflow = cJSON_CreateObject();

		cJSON_AddItemToObject(cdr, "callflow", j_callflow);

		if (!zstr(caller_profile->dialplan)) {
			cJSON_AddItemToObject(j_callflow, "dialplan", cJSON_CreateString((char *)caller_profile->dialplan));
		}

		if (!zstr(caller_profile->profile_index)) {
			cJSON_AddItemToObject(j_callflow, "profile_index", cJSON_CreateString((char *)caller_profile->profile_index));
		}

		if (caller_profile->caller_extension) {
			switch_caller_application_t *ap;

			j_caller_extension = cJSON_CreateObject();
			j_caller_extension_apps = cJSON_CreateArray();

			cJSON_AddItemToObject(j_callflow, "extension", j_caller_extension);

			cJSON_AddItemToObject(j_caller_extension, "name", cJSON_CreateString(caller_profile->caller_extension->extension_name));
			cJSON_AddItemToObject(j_caller_extension, "number", cJSON_CreateString(caller_profile->caller_extension->extension_number));
			cJSON_AddItemToObject(j_caller_extension, "applications", j_caller_extension_apps);

			if (caller_profile->caller_extension->current_application) {
				cJSON_AddItemToObject(j_caller_extension, "current_app", cJSON_CreateString(caller_profile->caller_extension->current_application->application_name));
			}

			for (ap = caller_profile->caller_extension->applications; ap; ap = ap->next) {
				j_application = cJSON_CreateObject();

				cJSON_AddItemToArray(j_caller_extension_apps, j_application);

				if (ap == caller_profile->caller_extension->current_application) {
					cJSON_AddItemToObject(j_application, "last_executed", cJSON_CreateString("true"));
				}
				cJSON_AddItemToObject(j_application, "app_name", cJSON_CreateString(ap->application_name));
				cJSON_AddItemToObject(j_application, "app_data", cJSON_CreateString(switch_str_nil(ap->application_data)));
			}

			if (caller_profile->caller_extension->children) {
				switch_caller_profile_t *cp = NULL;
				j_inner_extension = cJSON_CreateArray();
				cJSON_AddItemToObject(j_caller_extension, "sub_extensions", j_inner_extension);
				for (cp = caller_profile->caller_extension->children; cp; cp = cp->next) {

					if (!cp->caller_extension) {
						continue;
					}

					j_caller_extension = cJSON_CreateObject();
					cJSON_AddItemToArray(j_inner_extension, j_caller_extension);

					cJSON_AddItemToObject(j_caller_extension, "name", cJSON_CreateString(cp->caller_extension->extension_name));
					cJSON_AddItemToObject(j_caller_extension, "number", cJSON_CreateString(cp->caller_extension->extension_number));

					cJSON_AddItemToObject(j_caller_extension, "dialplan", cJSON_CreateString((char *)cp->dialplan));

					if (cp->caller_extension->current_application) {
						cJSON_AddItemToObject(j_caller_extension, "current_app", cJSON_CreateString(cp->caller_extension->current_application->application_name));
					}

					j_caller_extension_apps = cJSON_CreateArray();
					cJSON_AddItemToObject(j_caller_extension, "applications", j_caller_extension_apps);
					for (ap = cp->caller_extension->applications; ap; ap = ap->next) {
						j_application = cJSON_CreateObject();
						cJSON_AddItemToArray(j_caller_extension_apps, j_application);

						if (ap == cp->caller_extension->current_application) {
							cJSON_AddItemToObject(j_application, "last_executed", cJSON_CreateString("true"));
						}
						cJSON_AddItemToObject(j_application, "app_name", cJSON_CreateString(ap->application_name));
						cJSON_AddItemToObject(j_application, "app_data", cJSON_CreateString(switch_str_nil(ap->application_data)));
					}
				}
			}
		}

		j_main_cp = cJSON_CreateObject();
		cJSON_AddItemToObject(j_callflow, "caller_profile", j_main_cp);

		switch_ivr_set_json_profile_data(j_main_cp, caller_profile);

		if (caller_profile->originator_caller_profile) {
			switch_caller_profile_t *cp = NULL;

			j_o = cJSON_CreateObject();
			cJSON_AddItemToObject(j_main_cp, "originator", j_o);

			j_o_profiles = cJSON_CreateArray();
			cJSON_AddItemToObject(j_o, "originator_caller_profiles", j_o_profiles);

			for (cp = caller_profile->originator_caller_profile; cp; cp = cp->next) {
				j_caller_profile = cJSON_CreateObject();
				cJSON_AddItemToArray(j_o_profiles, j_caller_profile);

				switch_ivr_set_json_profile_data(j_caller_profile, cp);
			}
		}

		if (caller_profile->originatee_caller_profile) {
			switch_caller_profile_t *cp = NULL;

			j_o = cJSON_CreateObject();
			cJSON_AddItemToObject(j_main_cp, "originatee", j_o);

			j_o_profiles = cJSON_CreateArray();
			cJSON_AddItemToObject(j_o, "originatee_caller_profiles", j_o_profiles);

			for (cp = caller_profile->originatee_caller_profile; cp; cp = cp->next) {
				j_caller_profile = cJSON_CreateObject();
				cJSON_AddItemToArray(j_o_profiles, j_caller_profile);

				switch_ivr_set_json_profile_data(j_caller_profile, cp);
			}
		}

		if (caller_profile->times) {

			j_times = cJSON_CreateObject();
			cJSON_AddItemToObject(j_callflow, "times", j_times);

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->created);
			cJSON_AddItemToObject(j_times, "created_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->profile_created);
			cJSON_AddItemToObject(j_times, "profile_created_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->progress);
			cJSON_AddItemToObject(j_times, "progress_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->progress_media);
			cJSON_AddItemToObject(j_times, "progress_media_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->answered);
			cJSON_AddItemToObject(j_times, "answered_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->bridged);
			cJSON_AddItemToObject(j_times, "bridged_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->last_hold);
			cJSON_AddItemToObject(j_times, "last_hold_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->hold_accum);
			cJSON_AddItemToObject(j_times, "hold_accum_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->hungup);
			cJSON_AddItemToObject(j_times, "hangup_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->resurrected);
			cJSON_AddItemToObject(j_times, "resurrect_time", cJSON_CreateString(tmp));

			switch_snprintf(tmp, sizeof(tmp), "%" SWITCH_TIME_T_FMT, caller_profile->times->transferred);
			cJSON_AddItemToObject(j_times, "transfer_time", cJSON_CreateString(tmp));

		}

		caller_profile = caller_profile->next;
	}

	*json_cdr = cdr;

	return SWITCH_STATUS_SUCCESS;
	
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
	uint32_t interval;
	uint32_t ts = 0;
	switch_codec_implementation_t read_impl = { 0 };
	switch_core_session_get_read_impl(session, &read_impl);


	if (delay_ms < 1 || delay_ms > 10000) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid delay [%d] must be between 1 and 10000\n", delay_ms);
		return;
	}

	interval = read_impl.microseconds_per_packet / 1000;
	//samples = switch_samples_per_packet(read_impl.samples_per_second, interval);

	if (delay_ms < interval * 2) {
		delay_ms = interval * 2;
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Minimum possible delay for this codec (%d) has been chosen\n", delay_ms);
	}


	qlen = delay_ms / (interval);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "Setting delay to %dms (%d frames)\n", delay_ms, qlen);
	jb = stfu_n_init(qlen, qlen, read_impl.samples_per_packet, read_impl.samples_per_second, 0);

	write_frame.codec = switch_core_session_get_read_codec(session);

	while (switch_channel_ready(channel)) {
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		stfu_n_eat(jb, ts, 0, read_frame->payload, read_frame->data, read_frame->datalen, 0);
		ts += read_impl.samples_per_packet;

		if ((jb_frame = stfu_n_read_a_frame(jb))) {
			write_frame.data = jb_frame->data;
			write_frame.datalen = (uint32_t) jb_frame->dlen;
			write_frame.buflen = (uint32_t) jb_frame->dlen;
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
	const char *save_path = NULL, *chan_lang = NULL, *lang = NULL, *sound_path = NULL;
	switch_event_t *hint_data;
	switch_xml_t cfg, xml = NULL, language = NULL, macros = NULL, phrases = NULL;
	char *p;

	switch_assert(session);
	channel = switch_core_session_get_channel(session);
	switch_assert(channel);

	arg_recursion_check_start(args);


	if (zstr(module_name)) {
		module_name = "en";
	}

	if (module_name) {
		char *p;
		p = switch_core_session_strdup(session, module_name);
		module_name = p;
		
		if ((p = strchr(module_name, ':'))) {
			*p++ = '\0';
			chan_lang = p;
		}
	}

	if (!chan_lang) {
		lang = switch_channel_get_variable(channel, "language");

		if (!lang) {
			chan_lang = switch_channel_get_variable(channel, "default_language");
			if (!chan_lang) {
				chan_lang = module_name;
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "No language specified - Using [%s]\n", chan_lang);
		} else {
			chan_lang = lang;
		}
	}

	switch_event_create(&hint_data, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(hint_data);

	switch_event_add_header_string(hint_data, SWITCH_STACK_BOTTOM, "macro_name", "say_app");
	switch_event_add_header_string(hint_data, SWITCH_STACK_BOTTOM, "lang", chan_lang);
	switch_channel_event_set_data(channel, hint_data);

	if (switch_xml_locate_language(&xml, &cfg, hint_data, &language, &phrases, &macros, chan_lang) != SWITCH_STATUS_SUCCESS) {
		goto done;
	}

	if ((p = (char *) switch_xml_attr(language, "say-module"))) {
		module_name = switch_core_session_strdup(session, p);
	} else if ((p = (char *) switch_xml_attr(language, "module"))) {
		module_name = switch_core_session_strdup(session, p);
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Deprecated usage of module attribute\n");
	} else {
		module_name = chan_lang;
	}

	if (!(sound_path = (char *) switch_xml_attr(language, "sound-prefix"))) {
		if (!(sound_path = (char *) switch_xml_attr(language, "sound-path"))) {
			sound_path = (char *) switch_xml_attr(language, "sound_path");
		}
	}

	if (channel) {
		const char *p = switch_channel_get_variable(channel, "sound_prefix_enforced");
		if (!switch_true(p)) {
			save_path = switch_channel_get_variable(channel, "sound_prefix");
			if (sound_path) {
				switch_channel_set_variable(channel, "sound_prefix", sound_path);
			}
		}
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

	arg_recursion_check_stop(args);


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

SWITCH_DECLARE(switch_status_t) switch_ivr_say_string(switch_core_session_t *session,
													  const char *lang,
													  const char *ext,
													  const char *tosay,
													  const char *module_name,
													  const char *say_type,
													  const char *say_method,
													  const char *say_gender,
													  char **rstr)
{
	switch_say_interface_t *si;
	switch_channel_t *channel = NULL;
	switch_status_t status = SWITCH_STATUS_FALSE;
	const char *save_path = NULL, *chan_lang = NULL, *sound_path = NULL;
	switch_event_t *hint_data;
	switch_xml_t cfg, xml = NULL, language = NULL, macros = NULL, phrases = NULL;

	if (session) {
		channel = switch_core_session_get_channel(session);

		if (!lang) {
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
		}
	}

	if (!lang) lang = "en";
	if (!chan_lang) chan_lang = lang;

	switch_event_create(&hint_data, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(hint_data);

	switch_event_add_header_string(hint_data, SWITCH_STACK_BOTTOM, "macro_name", "say_app");
	switch_event_add_header_string(hint_data, SWITCH_STACK_BOTTOM, "lang", chan_lang);

	if (channel) {
		switch_channel_event_set_data(channel, hint_data);
	}

	if (switch_xml_locate_language(&xml, &cfg, hint_data, &language, &phrases, &macros, chan_lang) != SWITCH_STATUS_SUCCESS) {
		goto done;
	}

	if ((module_name = switch_xml_attr(language, "say-module"))) {
	} else if ((module_name = switch_xml_attr(language, "module"))) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Deprecated usage of module attribute\n");
	} else {
		module_name = chan_lang;
	}

	if (!(sound_path = (char *) switch_xml_attr(language, "sound-prefix"))) {
		if (!(sound_path = (char *) switch_xml_attr(language, "sound-path"))) {
			sound_path = (char *) switch_xml_attr(language, "sound_path");
		}
	}

	if (channel) {
		const char *p = switch_channel_get_variable(channel, "sound_prefix_enforced");	
		if (!switch_true(p)) {
			save_path = switch_channel_get_variable(channel, "sound_prefix");
			if (sound_path) {
				switch_channel_set_variable(channel, "sound_prefix", sound_path);
			}
		}
	}

	if ((si = switch_loadable_module_get_say_interface(module_name)) && si->say_string_function) {
		/* should go back and proto all the say mods to const.... */
		switch_say_args_t say_args = {0};
		
		say_args.type = switch_ivr_get_say_type_by_name(say_type);
		say_args.method = switch_ivr_get_say_method_by_name(say_method);
		say_args.gender = switch_ivr_get_say_gender_by_name(say_gender);
		say_args.ext = ext;
		status = si->say_string_function(session, (char *) tosay, &say_args, rstr);
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Invalid SAY Interface [%s]!\n", module_name);
		status = SWITCH_STATUS_FALSE;
	}

  done:

	if (hint_data) {
		switch_event_destroy(&hint_data);
	}

	if (save_path && channel) {
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

SWITCH_DECLARE(switch_status_t) switch_ivr_set_user_xml(switch_core_session_t *session, const char *prefix, 
														const char *user, const char *domain, switch_xml_t x_user)
{
	switch_xml_t x_params, x_param;
	char *number_alias;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_FALSE;

	char *prefix_buffer = NULL;
	size_t buffer_size = 0;
	size_t prefix_size = 0;


	status = SWITCH_STATUS_SUCCESS;

	if (!zstr(prefix)) {
		prefix_size = strlen(prefix);
		buffer_size = 1024 + prefix_size + 1;
		prefix_buffer = switch_core_session_alloc(session, buffer_size);
	}

	if ((number_alias = (char *) switch_xml_attr(x_user, "number-alias"))) {
		switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, "number_alias"), number_alias);
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

	if (switch_channel_get_caller_profile(channel) && (x_params = switch_xml_child(x_user, "profile-variables"))) {
		for (x_param = switch_xml_child(x_params, "variable"); x_param; x_param = x_param->next) {
			const char *var = switch_xml_attr(x_param, "name");
			const char *val = switch_xml_attr(x_param, "value");

			if (var && val) {
				switch_channel_set_profile_var(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, var), val);
			}
		}
	}

	if (user && domain) {
		switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, "user_name"), user);
		switch_channel_set_variable(channel, get_prefixed_str(prefix_buffer, buffer_size, prefix, prefix_size, "domain_name"), domain);
	}

	return status;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_set_user(switch_core_session_t *session, const char *data)
{
	switch_xml_t x_user = 0;
	char *user, *domain;
	switch_status_t status = SWITCH_STATUS_FALSE;

	char *prefix;

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


	if (switch_xml_locate_user_merged("id", user, domain, NULL, &x_user, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "can't find user [%s@%s]\n", user, domain);
		goto done;
	}

	status = switch_ivr_set_user_xml(session, prefix, user, domain, x_user);
	
	goto done;

  error:
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "No user@domain specified.\n");

  done:

	if (x_user) {
		switch_xml_free(x_user);
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

SWITCH_DECLARE(switch_bool_t) switch_ivr_uuid_force_exists(const char *uuid)
{
	switch_bool_t exists = SWITCH_FALSE;
	switch_core_session_t *psession = NULL;

	if ((psession = switch_core_session_force_locate(uuid))) {
		switch_core_session_rwunlock(psession);
		exists = 1;
	}

	return exists;
}

SWITCH_DECLARE(switch_status_t) switch_ivr_process_fh(switch_core_session_t *session, const char *cmd, switch_file_handle_t *fhp)
{
    if (zstr(cmd)) {
		return SWITCH_STATUS_SUCCESS;	
    }

	if (fhp) {
		if (!switch_test_flag(fhp, SWITCH_FILE_OPEN)) {
			return SWITCH_STATUS_FALSE;
		}

		if (!strncasecmp(cmd, "speed", 5)) {
			char *p;
		
			if ((p = strchr(cmd, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						step = 1;
					}
					fhp->speed += step;
				} else {
					int speed = atoi(p);
					fhp->speed = speed;
				}
				return SWITCH_STATUS_SUCCESS;
			}

			return SWITCH_STATUS_FALSE;

		} else if (!strncasecmp(cmd, "volume", 6)) {
			char *p;
			
			if ((p = strchr(cmd, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					if (!(step = atoi(p))) {
						step = 1;
					}
					fhp->vol += step;
				} else {
					int vol = atoi(p);
					fhp->vol = vol;
				}
				return SWITCH_STATUS_SUCCESS;
			}
			
			if (fhp->vol) {
				switch_normalize_volume(fhp->vol);
			}
			
			return SWITCH_STATUS_FALSE;
		} else if (!strcasecmp(cmd, "pause")) {
			if (switch_test_flag(fhp, SWITCH_FILE_PAUSE)) {
				switch_clear_flag(fhp, SWITCH_FILE_PAUSE);
			} else {
				switch_set_flag(fhp, SWITCH_FILE_PAUSE);
			}
			return SWITCH_STATUS_SUCCESS;
		} else if (!strcasecmp(cmd, "stop")) {
			switch_set_flag(fhp, SWITCH_FILE_DONE);
			return SWITCH_STATUS_FALSE;
		} else if (!strcasecmp(cmd, "truncate")) {
			switch_core_file_truncate(fhp, 0);
		} else if (!strcasecmp(cmd, "restart")) {
			unsigned int pos = 0;
			fhp->speed = 0;
			switch_core_file_seek(fhp, &pos, 0, SEEK_SET);
			return SWITCH_STATUS_SUCCESS;
		} else if (!strncasecmp(cmd, "seek", 4)) {
			//switch_codec_t *codec;
			unsigned int samps = 0;
			unsigned int pos = 0;
			char *p;
			//codec = switch_core_session_get_read_codec(session);
			
			if ((p = strchr(cmd, ':'))) {
				p++;
				if (*p == '+' || *p == '-') {
					int step;
					int32_t target;
					if (!(step = atoi(p))) {
						step = 1000;
					}

					samps = step * (fhp->native_rate / 1000);
					target = (int32_t)fhp->offset_pos + samps;

					if (target < 0) {
						target = 0;
					}

					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "seek to position %d\n", target);
					switch_core_file_seek(fhp, &pos, target, SEEK_SET);

				} else {
					samps = switch_atoui(p) * (fhp->native_rate / 1000);
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "seek to position %d\n", samps);
					switch_core_file_seek(fhp, &pos, samps, SEEK_SET);
				}
			}

			return SWITCH_STATUS_SUCCESS;
		}
	}

    if (!strcmp(cmd, "true") || !strcmp(cmd, "undefined")) {
		return SWITCH_STATUS_SUCCESS;
    }

    return SWITCH_STATUS_FALSE;
	
}

#define START_SAMPLES 32768

SWITCH_DECLARE(switch_status_t) switch_ivr_insert_file(switch_core_session_t *session, const char *file, const char *insert_file, switch_size_t sample_point)
{
	switch_file_handle_t orig_fh = { 0 };
	switch_file_handle_t new_fh = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	char *tmp_file;
	switch_uuid_t uuid;
	char uuid_str[SWITCH_UUID_FORMATTED_LENGTH + 1];
	int16_t *abuf = NULL;
	switch_size_t olen = 0;
	int asis = 0;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_size_t sample_count = 0;
	uint32_t pos = 0;
	char *ext;

	switch_uuid_get(&uuid);
	switch_uuid_format(uuid_str, &uuid);

	if ((ext = strrchr(file, '.'))) {
		ext++;
	} else {
		ext = "wav";
	}
	
	tmp_file = switch_core_session_sprintf(session, "%s%smsg_%s.%s", 
										   SWITCH_GLOBAL_dirs.temp_dir, SWITCH_PATH_SEPARATOR, uuid_str, ext);	
	
	switch_core_session_get_read_impl(session, &read_impl);
	
	new_fh.channels = read_impl.number_of_channels;
	new_fh.native_rate = read_impl.actual_samples_per_second;


	if (switch_core_file_open(&new_fh,
							  tmp_file,
							  new_fh.channels,
							  read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_WRITE | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open file %s\n", tmp_file);
		goto end;
	}


	if (switch_core_file_open(&orig_fh,
							  file,
							  new_fh.channels,
							  read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open file %s\n", file);
		goto end;
	}


	switch_zmalloc(abuf, START_SAMPLES * sizeof(*abuf));

	if (switch_test_flag((&orig_fh), SWITCH_FILE_NATIVE)) {
		asis = 1;
	}

	while (switch_channel_ready(channel)) {
		olen = START_SAMPLES;

		if (!asis) {
			olen /= 2;
		}

		if ((sample_count + olen) > sample_point) {
			olen = sample_point - sample_count;
		}

		if (!olen || switch_core_file_read(&orig_fh, abuf, &olen) != SWITCH_STATUS_SUCCESS || !olen) {
			break;
		}

		sample_count += olen;

		switch_core_file_write(&new_fh, abuf, &olen);
	}

	switch_core_file_close(&orig_fh);


	if (switch_core_file_open(&orig_fh,
							  insert_file,
							  new_fh.channels,
							  read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open file %s\n", file);
		goto end;
	}


	while (switch_channel_ready(channel)) {
		olen = START_SAMPLES;

		if (!asis) {
			olen /= 2;
		}

		if (switch_core_file_read(&orig_fh, abuf, &olen) != SWITCH_STATUS_SUCCESS || !olen) {
			break;
		}

		sample_count += olen;

		switch_core_file_write(&new_fh, abuf, &olen);
	}

	switch_core_file_close(&orig_fh);

	if (switch_core_file_open(&orig_fh,
							  file,
							  new_fh.channels,
							  read_impl.actual_samples_per_second, SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open file %s\n", file);
		goto end;
	}

	pos = 0;
	switch_core_file_seek(&orig_fh, &pos, sample_point, SEEK_SET);

	while (switch_channel_ready(channel)) {
		olen = START_SAMPLES;

		if (!asis) {
			olen /= 2;
		}

		if (switch_core_file_read(&orig_fh, abuf, &olen) != SWITCH_STATUS_SUCCESS || !olen) {
			break;
		}

		sample_count += olen;

		switch_core_file_write(&new_fh, abuf, &olen);
	}

 end:

	if (switch_test_flag((&orig_fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&orig_fh);
	}

	if (switch_test_flag((&new_fh), SWITCH_FILE_OPEN)) {
		switch_core_file_close(&new_fh);
	}

	switch_file_rename(tmp_file, file, switch_core_session_get_pool(session));
	unlink(tmp_file);

	switch_safe_free(abuf);

	return SWITCH_STATUS_SUCCESS;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_create_message_reply(switch_event_t **reply, switch_event_t *message, const char *new_proto)
{
	switch_status_t status = SWITCH_STATUS_SUCCESS;

	if ((status = switch_event_dup_reply(reply, message) != SWITCH_STATUS_SUCCESS)) {
		abort();
	}

	switch_event_add_header_string(*reply, SWITCH_STACK_BOTTOM, "proto", new_proto);

	return status;
}

SWITCH_DECLARE(char *) switch_ivr_check_presence_mapping(const char *exten_name, const char *domain_name)
{
	char *cf = "presence_map.conf";
	switch_xml_t cfg, xml, x_domains, x_domain, x_exten;
	char *r = NULL;
	switch_event_t *params = NULL;
	switch_regex_t *re = NULL;
	int proceed = 0, ovector[100];

	switch_event_create(&params, SWITCH_EVENT_REQUEST_PARAMS);
	switch_assert(params);

	if ( !zstr(domain_name) ) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "domain", domain_name);
	}

	if ( !zstr(exten_name) ) {
		switch_event_add_header_string(params, SWITCH_STACK_BOTTOM, "exten", exten_name);
	}

	if (!(xml = switch_xml_open_cfg(cf, &cfg, params))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Open of %s failed\n", cf);
		goto end;
	}

	if (!(x_domains = switch_xml_child(cfg, "domains"))) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Can't find any domains!\n");
		goto end;
	}

	for (x_domain = switch_xml_child(x_domains, "domain"); x_domain; x_domain = x_domain->next) {
		const char *dname = switch_xml_attr(x_domain, "name");
		if (!dname || (strcasecmp(dname, "*") && strcasecmp(domain_name, dname))) continue;
		
		for (x_exten = switch_xml_child(x_domain, "exten"); x_exten; x_exten = x_exten->next) {
			const char *regex = switch_xml_attr(x_exten, "regex");
			const char *proto = switch_xml_attr(x_exten, "proto");
			
			if (!zstr(regex) && !zstr(proto)) {
				proceed = switch_regex_perform(exten_name, regex, &re, ovector, sizeof(ovector) / sizeof(ovector[0]));
				switch_regex_safe_free(re);
				
				if (proceed) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG1, "Mapping %s@%s to proto %s matching expression [%s]\n", 
									  exten_name, domain_name, proto, regex);
					r = strdup(proto);
					goto end;
				}
				
			}
		}
	}

 end:
	switch_event_destroy(&params);

	if (xml) {
		switch_xml_free(xml);
	}

	return r;
	
}

SWITCH_DECLARE(switch_status_t) switch_ivr_kill_uuid(const char *uuid, switch_call_cause_t cause)
{
	switch_core_session_t *session;

	if (zstr(uuid) || !(session = switch_core_session_locate(uuid))) {
		return SWITCH_STATUS_FALSE;
	} else {
		switch_channel_t *channel = switch_core_session_get_channel(session);
		switch_channel_hangup(channel, cause);
		switch_core_session_rwunlock(session);
		return SWITCH_STATUS_SUCCESS;
	}
}

SWITCH_DECLARE(switch_status_t) switch_ivr_blind_transfer_ack(switch_core_session_t *session, switch_bool_t success)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_status_t status = SWITCH_STATUS_FALSE;

	if (switch_channel_test_flag(channel, CF_CONFIRM_BLIND_TRANSFER)) {
		switch_core_session_t *other_session;
		const char *uuid = switch_channel_get_variable(channel, "blind_transfer_uuid");

		switch_channel_clear_flag(channel, CF_CONFIRM_BLIND_TRANSFER);

		if (!zstr(uuid) && (other_session = switch_core_session_locate(uuid))) {
			switch_core_session_message_t msg = { 0 };			
			msg.message_id = SWITCH_MESSAGE_INDICATE_BLIND_TRANSFER_RESPONSE;
			msg.from = __FILE__;
			msg.numeric_arg = success;
			switch_core_session_receive_message(other_session, &msg);
			switch_core_session_rwunlock(other_session);
			status = SWITCH_STATUS_SUCCESS;
		}
	}

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
