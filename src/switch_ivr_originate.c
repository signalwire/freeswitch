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
 *
 * switch_ivr_originate.c -- IVR Library (originate)
 *
 */

#include <switch.h>

static const switch_state_handler_table_t originate_state_handlers;

static switch_status_t originate_on_consume_media_transmit(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if (!switch_channel_test_flag(channel, CF_PROXY_MODE)) {
		while (switch_channel_get_state(channel) == CS_CONSUME_MEDIA && !switch_channel_test_flag(channel, CF_TAGGED)) {
			switch_ivr_sleep(session, 10, NULL);
		}
	}

	switch_channel_clear_state_handler(channel, &originate_state_handlers);

	return SWITCH_STATUS_FALSE;
}

static switch_status_t originate_on_routing(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);

	/* put the channel in a passive state until it is answered */
	switch_channel_set_state(channel, CS_CONSUME_MEDIA);

	return SWITCH_STATUS_FALSE;
}

static const switch_state_handler_table_t originate_state_handlers = {
	/*.on_init */ NULL,
	/*.on_routing */ originate_on_routing,
	/*.on_execute */ NULL,
	/*.on_hangup */ NULL,
	/*.on_exchange_media */ NULL,
	/*.on_soft_execute */ originate_on_consume_media_transmit,
	/*.on_consume_media */ originate_on_consume_media_transmit
};

typedef enum {
	IDX_TIMEOUT = -3,
	IDX_CANCEL = -2,
	IDX_NADA = -1
} abort_t;

struct key_collect {
	char *key;
	char *file;
	switch_core_session_t *session;
};

static void *SWITCH_THREAD_FUNC collect_thread_run(switch_thread_t *thread, void *obj)
{
	struct key_collect *collect = (struct key_collect *) obj;
	switch_channel_t *channel = switch_core_session_get_channel(collect->session);
	char buf[10] = SWITCH_BLANK_STRING;
	char *p, term;

	if (!strcasecmp(collect->key, "exec")) {
		char *data;
		const switch_application_interface_t *application_interface;
		char *app_name, *app_data;

		if (!(data = collect->file)) {
			goto wbreak;
		}

		app_name = data;

		if ((app_data = strchr(app_name, ' '))) {
			*app_data++ = '\0';
		}

		if ((application_interface = switch_loadable_module_get_application_interface(app_name)) == 0) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Invalid Application %s\n", app_name);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			goto wbreak;
		}

		if (!application_interface->application_function) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "No Function for %s\n", app_name);
			switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
			goto wbreak;
		}

		switch_core_session_exec(collect->session, application_interface, app_data);

		if (switch_channel_get_state(channel) < CS_HANGUP) {
			switch_channel_set_flag(channel, CF_WINNER);
		}
		goto wbreak;
	}

	if (!switch_channel_ready(channel)) {
		switch_channel_hangup(channel, SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER);
		goto wbreak;
	}

	while (switch_channel_ready(channel)) {
		memset(buf, 0, sizeof(buf));

		if (collect->file) {
			switch_input_args_t args = { 0 };
			args.buf = buf;
			args.buflen = sizeof(buf);
			switch_ivr_play_file(collect->session, NULL, collect->file, &args);
		} else {
			switch_ivr_collect_digits_count(collect->session, buf, sizeof(buf), 1, SWITCH_BLANK_STRING, &term, 0, 0, 0);
		}

		for (p = buf; *p; p++) {
			if (*collect->key == *p) {
				switch_channel_set_flag(channel, CF_WINNER);
				goto wbreak;
			}
		}
	}
  wbreak:

	return NULL;
}

static void launch_collect_thread(struct key_collect *collect)
{
	switch_thread_t *thread;
	switch_threadattr_t *thd_attr = NULL;

	switch_threadattr_create(&thd_attr, switch_core_session_get_pool(collect->session));
	switch_threadattr_detach_set(thd_attr, 1);
	switch_threadattr_stacksize_set(thd_attr, SWITCH_THREAD_STACKSIZE);
	switch_thread_create(&thread, thd_attr, collect_thread_run, collect, switch_core_session_get_pool(collect->session));
}

static uint8_t check_channel_status(switch_channel_t **peer_channels,
									switch_core_session_t **peer_sessions,
									uint32_t len, int32_t *idx, uint32_t *hups, char *file, char *key, uint8_t early_ok, 
									uint8_t *ring_ready, uint8_t *progress,
									uint8_t return_ring_ready)
{

	uint32_t i;
	*hups = 0;
	*idx = IDX_NADA;

	for (i = 0; i < len; i++) {
		switch_channel_state_t state;
		if (!peer_channels[i]) {
			continue;
		}
		if (!*ring_ready && switch_channel_test_flag(peer_channels[i], CF_RING_READY)) {
			*ring_ready = 1;
		}
		if (!*ring_ready && switch_channel_test_flag(peer_channels[i], CF_EARLY_MEDIA)) {
			*progress = 1;
		}

		state = switch_channel_get_state(peer_channels[i]);
		if (state >= CS_HANGUP || state == CS_RESET || switch_channel_test_flag(peer_channels[i], CF_TRANSFER) ||
			switch_channel_test_flag(peer_channels[i], CF_BRIDGED) || !switch_channel_test_flag(peer_channels[i], CF_ORIGINATING)
			) {
			(*hups)++;
		} else if ((switch_channel_test_flag(peer_channels[i], CF_ANSWERED) ||
					(early_ok && switch_channel_test_flag(peer_channels[i], CF_EARLY_MEDIA)) ||
					(*ring_ready && return_ring_ready && len == 1 && switch_channel_test_flag(peer_channels[i], CF_RING_READY))
				   )
				   && !switch_channel_test_flag(peer_channels[i], CF_TAGGED)
			) {

			if (!switch_strlen_zero(key)) {
				struct key_collect *collect;

				if ((collect = switch_core_session_alloc(peer_sessions[i], sizeof(*collect)))) {
					switch_channel_set_flag(peer_channels[i], CF_TAGGED);
					collect->key = key;
					if (!switch_strlen_zero(file)) {
						collect->file = switch_core_session_strdup(peer_sessions[i], file);
					}

					collect->session = peer_sessions[i];
					launch_collect_thread(collect);
				}
			} else {
				*idx = i;
				return 0;

			}
		} else if (switch_channel_test_flag(peer_channels[i], CF_WINNER)) {
			*idx = i;
			return 0;
		}
	}

	if (*hups == len) {
		return 0;
	} else {
		return 1;
	}
}

struct ringback {
	switch_buffer_t *audio_buffer;
	teletone_generation_session_t ts;
	switch_file_handle_t fhb;
	switch_file_handle_t *fh;
	uint8_t asis;
};

typedef struct ringback ringback_t;

static int teletone_handler(teletone_generation_session_t *ts, teletone_tone_map_t *map)
{
	ringback_t *tto = ts->user_data;
	int wrote;

	if (!tto) {
		return -1;
	}
	wrote = teletone_mux_tones(ts, map);
	switch_buffer_write(tto->audio_buffer, ts->buffer, wrote * 2);

	return 0;
}


SWITCH_DECLARE(switch_status_t) switch_ivr_wait_for_answer(switch_core_session_t *session, switch_core_session_t *peer_session)
{
	switch_channel_t *caller_channel = switch_core_session_get_channel(session);
	switch_channel_t *peer_channel = switch_core_session_get_channel(peer_session);
	const char *ringback_data = NULL;
	switch_frame_t write_frame = { 0 };
	switch_codec_t write_codec = { 0 };
	switch_codec_t *read_codec = switch_core_session_get_read_codec(session);
	uint8_t pass = 0;
	ringback_t ringback = { 0 };
	switch_core_session_message_t *message = NULL;
	switch_frame_t *read_frame = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	int timelimit = 60;
	const char *var = switch_channel_get_variable(caller_channel, "call_timeout");
	switch_time_t start = 0;

	if ((switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA))) {
		return SWITCH_STATUS_SUCCESS;
	}

	switch_zmalloc(write_frame.data, SWITCH_RECOMMENDED_BUFFER_SIZE);
	write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	if (var) {
		timelimit = atoi(var);
		if (timelimit < 0) {
			timelimit = 60;
		}
	}

	timelimit *= 1000000;
	start = switch_timestamp_now();

	if (switch_channel_test_flag(caller_channel, CF_ANSWERED)) {
		ringback_data = switch_channel_get_variable(caller_channel, "transfer_ringback");
	}

	if (!ringback_data) {
		ringback_data = switch_channel_get_variable(caller_channel, "ringback");
	}


	if (read_codec && (ringback_data ||
					   (!(switch_channel_test_flag(caller_channel, CF_PROXY_MODE) && switch_channel_test_flag(caller_channel, CF_PROXY_MEDIA))))) {
		if (!(pass = (uint8_t) switch_test_flag(read_codec, SWITCH_CODEC_FLAG_PASSTHROUGH))) {
			if (switch_core_codec_init(&write_codec,
									   "L16",
									   NULL,
									   read_codec->implementation->actual_samples_per_second,
									   read_codec->implementation->microseconds_per_frame / 1000,
									   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
									   switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {


				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
								  "Raw Codec Activation Success L16@%uhz 1 channel %dms\n",
								  read_codec->implementation->actual_samples_per_second, read_codec->implementation->microseconds_per_frame / 1000);

				write_frame.codec = &write_codec;
				write_frame.datalen = read_codec->implementation->bytes_per_frame;
				write_frame.samples = write_frame.datalen / 2;
				memset(write_frame.data, 255, write_frame.datalen);

				if (ringback_data) {
					char *tmp_data = NULL;

					switch_buffer_create_dynamic(&ringback.audio_buffer, 512, 1024, 0);
					switch_buffer_set_loops(ringback.audio_buffer, -1);

					if (switch_is_file_path(ringback_data)) {
						char *ext;

						if (strrchr(ringback_data, '.') || strstr(ringback_data, SWITCH_URL_SEPARATOR)) {
							switch_core_session_set_read_codec(session, &write_codec);
						} else {
							ringback.asis++;
							write_frame.codec = read_codec;
							ext = read_codec->implementation->iananame;
							tmp_data = switch_mprintf("%s.%s", ringback_data, ext);
							ringback_data = tmp_data;
						}

						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Play Ringback File [%s]\n", ringback_data);

						ringback.fhb.channels = read_codec->implementation->number_of_channels;
						ringback.fhb.samplerate = read_codec->implementation->actual_samples_per_second;
						if (switch_core_file_open(&ringback.fhb,
												  ringback_data,
												  read_codec->implementation->number_of_channels,
												  read_codec->implementation->actual_samples_per_second,
												  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Playing File\n");
							switch_safe_free(tmp_data);
							goto done;
						}
						ringback.fh = &ringback.fhb;
					} else {
						teletone_init_session(&ringback.ts, 0, teletone_handler, &ringback);
						ringback.ts.rate = read_codec->implementation->actual_samples_per_second;
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Play Ringback Tone [%s]\n", ringback_data);
						if (teletone_run(&ringback.ts, ringback_data)) {
							switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Playing Tone\n");
							teletone_destroy_session(&ringback.ts);
							switch_buffer_destroy(&ringback.audio_buffer);
							ringback_data = NULL;
						}
					}
					switch_safe_free(tmp_data);
				}
			} else {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec Error!\n");
				switch_channel_hangup(caller_channel, SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE);
				read_codec = NULL;
			}
		}
	}

	while (switch_channel_ready(peer_channel)
		   && !(switch_channel_test_flag(peer_channel, CF_ANSWERED) || switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA))) {
		int diff = (int) (switch_timestamp_now() - start);

		if (diff > timelimit) {
			status = SWITCH_STATUS_TIMEOUT;
			goto done;
		}

		if (switch_core_session_dequeue_message(peer_session, &message) == SWITCH_STATUS_SUCCESS) {
			if (switch_test_flag(message, SCSMF_DYNAMIC)) {
				switch_safe_free(message);
			} else {
				message = NULL;
			}
		}
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}


		if (read_frame && !pass) {
			if (ringback.fh) {
				switch_size_t mlen, olen;
				unsigned int pos = 0;

				if (ringback.asis) {
					mlen = write_frame.codec->implementation->encoded_bytes_per_frame;
				} else {
					mlen = write_frame.codec->implementation->samples_per_frame;
				}

				olen = mlen;
				switch_core_file_read(ringback.fh, write_frame.data, &olen);

				if (olen == 0) {
					olen = mlen;
					ringback.fh->speed = 0;
					switch_core_file_seek(ringback.fh, &pos, 0, SEEK_SET);
					switch_core_file_read(ringback.fh, write_frame.data, &olen);
					if (olen == 0) {
						break;
					}
				}
				write_frame.datalen = (uint32_t) (ringback.asis ? olen : olen * 2);
			} else if (ringback.audio_buffer) {
				if ((write_frame.datalen = (uint32_t) switch_buffer_read_loop(ringback.audio_buffer,
																			  write_frame.data,
																			  write_frame.codec->implementation->bytes_per_frame)) <= 0) {
					break;
				}
			}

			if (switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
				break;
			}
		} else {
			switch_yield(1000);
		}
	}

  done:

	if (ringback.fh) {
		switch_core_file_close(ringback.fh);
		ringback.fh = NULL;
	} else if (ringback.audio_buffer) {
		teletone_destroy_session(&ringback.ts);
		switch_buffer_destroy(&ringback.audio_buffer);
	}

	switch_core_session_reset(session, SWITCH_TRUE);

	if (write_codec.implementation) {
		switch_core_codec_destroy(&write_codec);
	}

	switch_safe_free(write_frame.data);

	return status;
}

static void process_import(switch_core_session_t *session, switch_channel_t *peer_channel)
{
	const char *import, *val;
	switch_channel_t *caller_channel;

	switch_assert(session && peer_channel);
	caller_channel = switch_core_session_get_channel(session);
	
	if ((import = switch_channel_get_variable(caller_channel, "import"))) {
		char *mydata = switch_core_session_strdup(session, import);
		int i, argc;
		char *argv[64] = { 0 };

		if ((argc = switch_separate_string(mydata, ',', argv, (sizeof(argv) / sizeof(argv[0]))))) {
			for(i = 0; i < argc; i++) {
				if ((val = switch_channel_get_variable(peer_channel, argv[i]))) {
					switch_channel_set_variable(caller_channel, argv[i], val);
				}
			}
		}
	}
}

#define MAX_PEERS 128
SWITCH_DECLARE(switch_status_t) switch_ivr_originate(switch_core_session_t *session,
													 switch_core_session_t **bleg,
													 switch_call_cause_t *cause,
													 const char *bridgeto,
													 uint32_t timelimit_sec,
													 const switch_state_handler_table_t *table,
													 const char *cid_name_override,
													 const char *cid_num_override,
													 switch_caller_profile_t *caller_profile_override, switch_originate_flag_t flags)
{
	switch_originate_flag_t myflags = SOF_NONE;
	char *pipe_names[MAX_PEERS] = { 0 };
	char *data = NULL;
	switch_status_t status = SWITCH_STATUS_SUCCESS;
	switch_channel_t *caller_channel = NULL;
	char *peer_names[MAX_PEERS] = { 0 };
	switch_core_session_t *new_session = NULL, *peer_session, *peer_sessions[MAX_PEERS] = { 0 };
	switch_caller_profile_t *new_profile = NULL, *caller_profiles[MAX_PEERS] = { 0 }, *caller_caller_profile;
	char *chan_type = NULL, *chan_data;
	switch_channel_t *peer_channel = NULL, *peer_channels[MAX_PEERS] = { 0 };
	ringback_t ringback = { 0 };
	time_t start;
	switch_frame_t *read_frame = NULL;
	switch_memory_pool_t *pool = NULL;
	int r = 0, i, and_argc = 0, or_argc = 0;
	int32_t sleep_ms = 1000, try = 0, retries = 1, idx = IDX_NADA;
	switch_codec_t write_codec = { 0 };
	switch_frame_t write_frame = { 0 };
	uint8_t pass = 0;
	char key[80] = SWITCH_BLANK_STRING, file[512] = SWITCH_BLANK_STRING, *odata, *var;
	switch_call_cause_t reason = SWITCH_CAUSE_NONE;
	uint8_t to = 0;
	char *var_val, *vars = NULL;
	const char *ringback_data = NULL;
	switch_codec_t *read_codec = NULL;
	uint8_t sent_ring = 0, early_ok = 1, return_ring_ready = 0, progress = 0;
	switch_core_session_message_t *message = NULL;
	switch_event_t *var_event = NULL;
	uint8_t fail_on_single_reject = 0;
	uint8_t ring_ready = 0;
	char *loop_data = NULL;
	uint32_t progress_timelimit_sec = 0;

	switch_zmalloc(write_frame.data, SWITCH_RECOMMENDED_BUFFER_SIZE);
	write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	*bleg = NULL;
	odata = strdup(bridgeto);

	if (!odata) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Memory Error!\n");
		status = SWITCH_STATUS_MEMERR;
		goto done;
	}

	data = odata;

	/* strip leading spaces */
	while (data && *data && *data == ' ') {
		data++;
	}

	if (*data == '{') {
		char *e = switch_find_end_paren(data, '{', '}');

		if (e) {
			vars = data + 1;
			*e++ = '\0';
			data = e;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Parse Error!\n");
			status = SWITCH_STATUS_GENERR;
			goto done;
		}
	}

	/* strip leading spaces (again) */
	while (data && *data && *data == ' ') {
		data++;
	}

	if (switch_strlen_zero(data)) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "No origination URL specified!\n");
		status = SWITCH_STATUS_GENERR;
		goto done;
	}

	/* Some channel are created from an originating channel and some aren't so not all outgoing calls have a way to get params
	   so we will normalize dialstring params and channel variables (when there is an originator) into an event that we 
	   will use as a pseudo hash to consult for params as needed.
	 */
	if (switch_event_create(&var_event, SWITCH_EVENT_MESSAGE) != SWITCH_STATUS_SUCCESS) {
		abort();
	}

	if (session) {
		switch_event_header_t *hi;
		caller_channel = switch_core_session_get_channel(session);

		/* Copy all the applicable channel variables into the event */
		if ((hi = switch_channel_variable_first(caller_channel))) {
			for (; hi; hi = hi->next) {
				int ok = 0;
				if (!strcasecmp((char *) hi->name, "group_confirm_key")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "group_confirm_file")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "forked_dial")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "fail_on_single_reject")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "ignore_early_media")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "return_ring_ready")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "originate_retries")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "originate_timeout")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "progress_timeout")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "originate_retry_sleep_ms")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "origination_caller_id_name")) {
					ok = 1;
				} else if (!strcasecmp((char *) hi->name, "origination_caller_id_number")) {
					ok = 1;
				}

				if (ok) {
					switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, (char *) hi->name, "%s", (char *) hi->value);
				}
			}
			switch_channel_variable_last(caller_channel);
		}
		/*
		   if ((hi = switch_channel_variable_first(caller_channel))) {
		   for (; hi; hi = switch_hash_next(hi)) {
		   switch_hash_this(hi, &vvar, NULL, &vval);
		   if (vvar && vval) {
		   switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, (void *) vvar, "%s", (char *) vval);
		   }
		   }
		   switch_channel_variable_last(caller_channel);
		   }
		 */
	}

	if (vars) {					/* Parse parameters specified from the dialstring */
		char *var_array[1024] = { 0 };
		int var_count = 0;
		if ((var_count = switch_separate_string(vars, ',', var_array, (sizeof(var_array) / sizeof(var_array[0]))))) {
			int x = 0;
			for (x = 0; x < var_count; x++) {
				char *inner_var_array[2] = { 0 };
				int inner_var_count;
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "variable string %d = [%s]\n", x, var_array[x]);
				if ((inner_var_count =
					 switch_separate_string(var_array[x], '=', inner_var_array, (sizeof(inner_var_array) / sizeof(inner_var_array[0])))) == 2) {

					switch_event_add_header(var_event, SWITCH_STACK_BOTTOM, inner_var_array[0], "%s", inner_var_array[1]);
				}
			}
		}
	}

	if (caller_channel) {		/* ringback is only useful when there is an originator */
		ringback_data = NULL;

		if (switch_channel_test_flag(caller_channel, CF_ANSWERED)) {
			ringback_data = switch_channel_get_variable(caller_channel, "transfer_ringback");
		}

		if (!ringback_data) {
			ringback_data = switch_channel_get_variable(caller_channel, "ringback");
		}

		switch_channel_set_variable(caller_channel, "originate_disposition", "failure");
	}

	if ((var = switch_event_get_header(var_event, "group_confirm_key"))) {
		switch_copy_string(key, var, sizeof(key));
		if ((var = switch_event_get_header(var_event, "group_confirm_file"))) {
			switch_copy_string(file, var, sizeof(file));
		}
	}
	// When using the AND operator, the fail_on_single_reject flag may be set in order to indicate that a single
	// rejections should terminate the attempt rather than a timeout, answer, or rejection by all.
	if ((var = switch_event_get_header(var_event, "fail_on_single_reject")) && switch_true(var)) {
		fail_on_single_reject = 1;
	}

	if ((*file != '\0') && (!strcmp(file, "undef"))) {
		*file = '\0';
	}

	if ((var_val = switch_event_get_header(var_event, "ignore_early_media")) && switch_true(var_val)) {
		early_ok = 0;
	}

	if ((var_val = switch_event_get_header(var_event, "return_ring_ready")) && switch_true(var_val)) {
		return_ring_ready = 1;
	}

	if ((var_val = switch_event_get_header(var_event, "originate_timeout"))) {
		int tmp = atoi(var_val);
		if (tmp > 0) {
			timelimit_sec = (uint32_t) tmp;
		}
	}

	if ((var_val = switch_event_get_header(var_event, "progress_timeout"))) {
		int tmp = atoi(var_val);
		if (tmp > 0) {
			progress_timelimit_sec = (uint32_t) tmp;
		}
	}

	if ((var_val = switch_event_get_header(var_event, "originate_retries")) && switch_true(var_val)) {
		int32_t tmp;
		tmp = atoi(var_val);
		if (tmp > 0 && tmp < 101) {
			retries = tmp;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "Invalid originate_retries setting of %d ignored, value must be between 1 and 100\n", tmp);
		}
	}

	if ((var_val = switch_event_get_header(var_event, "originate_retry_sleep_ms")) && switch_true(var_val)) {
		int32_t tmp;
		tmp = atoi(var_val);
		if (tmp >= 500 && tmp <= 60000) {
			sleep_ms = tmp;
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING,
							  "Invalid originate_retry_sleep_ms setting of %d ignored, value must be between 500 and 60000\n", tmp);
		}
	}

	if (cid_name_override) {
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_caller_id_name", cid_name_override);
	} else {
		cid_name_override = switch_event_get_header(var_event, "origination_caller_id_name");
	}

	if (cid_num_override) {
		switch_event_add_header_string(var_event, SWITCH_STACK_BOTTOM, "origination_caller_id_number", cid_num_override);
	} else {
		cid_num_override = switch_event_get_header(var_event, "origination_caller_id_number");
	}

	if (!progress_timelimit_sec) {
		progress_timelimit_sec = timelimit_sec;
	}

	for (try = 0; try < retries; try++) {
		switch_safe_free(loop_data);
		loop_data = strdup(data);
		switch_assert(loop_data);
		or_argc = switch_separate_string(loop_data, '|', pipe_names, (sizeof(pipe_names) / sizeof(pipe_names[0])));

		if ((flags & SOF_NOBLOCK) && or_argc > 1) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Only calling the first element in the list in this mode.\n");
			or_argc = 1;
		}

		for (r = 0; r < or_argc; r++) {
			uint32_t hups;
			char *p, *e = NULL;
			const char *var_begin, *var_end;
			
			reason = SWITCH_CAUSE_NONE;
			memset(peer_names, 0, sizeof(peer_names));
			peer_session = NULL;
			memset(peer_sessions, 0, sizeof(peer_sessions));
			memset(peer_channels, 0, sizeof(peer_channels));
			memset(caller_profiles, 0, sizeof(caller_profiles));
			new_profile = NULL;
			new_session = NULL;
			chan_type = NULL;
			chan_data = NULL;
			peer_channel = NULL;
			start = 0;
			read_frame = NULL;
			pool = NULL;
			pass = 0;
			var = NULL;
			to = 0;
			sent_ring = 0;
			progress = 0;

			if (try > 0) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_NOTICE, "Originate attempt %d/%d in %d ms\n", try + 1, retries, sleep_ms);
				switch_yield(sleep_ms * 1000);
			}
			
			p = pipe_names[r];
			while(p && *p) {
				if (*p == '[') {
					e = switch_find_end_paren(p, '[', ']');
				}

				if (e && p && *p == ',') {
					*p = '|';
				}

				if (p == e) {
					e = NULL;
				}

				p++;
			}
			
			and_argc = switch_separate_string(pipe_names[r], ',', peer_names, (sizeof(peer_names) / sizeof(peer_names[0])));

			if ((flags & SOF_NOBLOCK) && and_argc > 1) {
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_WARNING, "Only calling the first element in the list in this mode.\n");
				and_argc = 1;
			}

			for (i = 0; i < and_argc; i++) {
				char *vdata;
				e = NULL;
				chan_type = peer_names[i];

				while (chan_type && *chan_type && *chan_type == ' ') {
					chan_type++;
				}

				vdata = chan_type;
				e = switch_find_end_paren(vdata, '[', ']');

				if (e) {
					vdata++;
					*e++ = '\0';
					chan_type = e;
				} else {
					vdata = NULL;
				}

				if ((chan_data = strchr(chan_type, '/')) != 0) {
					*chan_data = '\0';
					chan_data++;
				}

				if (session) {
					if (!switch_channel_ready(caller_channel)) {
						status = SWITCH_STATUS_FALSE;
						goto done;
					}

					caller_caller_profile = caller_profile_override ? caller_profile_override : switch_channel_get_caller_profile(caller_channel);
					new_profile = switch_caller_profile_clone(session, caller_caller_profile);
					new_profile->uuid = SWITCH_BLANK_STRING;
					new_profile->chan_name = SWITCH_BLANK_STRING;
					new_profile->destination_number = switch_core_strdup(new_profile->pool, chan_data);

					if (cid_name_override) {
						new_profile->caller_id_name = switch_core_strdup(new_profile->pool, cid_name_override);
					}
					if (cid_num_override) {
						new_profile->caller_id_number = switch_core_strdup(new_profile->pool, cid_num_override);
					}

					pool = NULL;
				} else {
					switch_core_new_memory_pool(&pool);

					if (caller_profile_override) {
						new_profile = switch_caller_profile_dup(pool, caller_profile_override);
						new_profile->destination_number = switch_core_strdup(new_profile->pool, switch_str_nil(chan_data));
						new_profile->uuid = SWITCH_BLANK_STRING;
						new_profile->chan_name = SWITCH_BLANK_STRING;
					} else {
						if (!cid_name_override) {
							cid_name_override = "FreeSWITCH";
						}
						if (!cid_num_override) {
							cid_num_override = "0000000000";
						}

						new_profile = switch_caller_profile_new(pool,
																NULL,
																NULL,
																cid_name_override, cid_num_override, NULL, NULL, NULL, NULL, __FILE__, NULL, chan_data);
					}
				}

				caller_profiles[i] = NULL;
				peer_channels[i] = NULL;
				peer_sessions[i] = NULL;
				new_session = NULL;

				if (and_argc > 1 || or_argc > 1) {
					myflags |= SOF_FORKED_DIAL;
				} else if (var_event) {
					const char *vvar;
					if ((vvar = switch_event_get_header(var_event, "forked_dial")) && switch_true(vvar)) {
						myflags |= SOF_FORKED_DIAL;
					}
				}

				if (vdata && (var_begin = switch_stristr("origination_caller_id_number=", vdata))) {
					char tmp[512] = "";
					var_begin += strlen("origination_caller_id_number=");
					var_end = strchr(var_begin, '|');
					if (var_end) {
						strncpy(tmp, var_begin, var_end-var_begin);
					} else {
						strncpy(tmp, var_begin, strlen(var_begin));
					}
					new_profile->caller_id_number = switch_core_strdup(new_profile->pool, tmp);
				}

				if (vdata && (var_begin = switch_stristr("origination_caller_id_name=", vdata))) {
					char tmp[512] = "";
					var_begin += strlen("origination_caller_id_name=");
					var_end = strchr(var_begin, '|');
					if (var_end) {
						strncpy(tmp, var_begin, var_end-var_begin);
					} else {
						strncpy(tmp, var_begin, strlen(var_begin));
					}
					new_profile->caller_id_name = switch_core_strdup(new_profile->pool, tmp);
				}

				if ((reason =
					 switch_core_session_outgoing_channel(session, var_event, chan_type, new_profile, &new_session, &pool,
														  myflags)) != SWITCH_CAUSE_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Cannot create outgoing channel of type [%s] cause: [%s]\n", chan_type,
									  switch_channel_cause2str(reason));
					if (pool) {
						switch_core_destroy_memory_pool(&pool);
					}
					continue;
				}

				switch_core_session_read_lock(new_session);
				pool = NULL;

				caller_profiles[i] = new_profile;
				peer_sessions[i] = new_session;
				peer_channels[i] = switch_core_session_get_channel(new_session);
				switch_channel_set_flag(peer_channels[i], CF_ORIGINATING);

				if (vdata) {
					char *var_array[1024] = { 0 };
					int var_count = 0;
					if ((var_count = switch_separate_string(vdata, '|', var_array, (sizeof(var_array) / sizeof(var_array[0]))))) {
						int x = 0;
						for (x = 0; x < var_count; x++) {
							char *inner_var_array[2] = { 0 };
							int inner_var_count;
							if ((inner_var_count =
								 switch_separate_string(var_array[x], '=',
														inner_var_array, (sizeof(inner_var_array) / sizeof(inner_var_array[0])))) == 2) {

								switch_channel_set_variable(peer_channels[i], inner_var_array[0], inner_var_array[1]);
							}
						}
					}
				}

				if (var_event) {
					switch_event_t *event;
					switch_event_header_t *header;
					/* install the vars from the {} params */
					for (header = var_event->headers; header; header = header->next) {
						switch_channel_set_variable(peer_channels[i], header->name, header->value);
					}
					switch_event_create(&event, SWITCH_EVENT_CHANNEL_ORIGINATE);
					switch_assert(event);
					switch_channel_event_set_data(peer_channels[i], event);
					switch_event_fire(&event);
				}

				if (!table) {
					table = &originate_state_handlers;
				}

				if (table) {
					switch_channel_add_state_handler(peer_channels[i], table);
				}

				if ((flags & SOF_NOBLOCK) && peer_sessions[i]) {
					status = SWITCH_STATUS_SUCCESS;
					*bleg = peer_sessions[i];
					*cause = SWITCH_CAUSE_SUCCESS;
					goto outer_for;
				}

				if (!switch_core_session_running(peer_sessions[i])) {
					//if (!(flags & SOF_NOBLOCK)) {
					//switch_channel_set_state(peer_channels[i], CS_ROUTING);
					//}
					//} else {
					switch_core_session_thread_launch(peer_sessions[i]);
				}
			}

			switch_timestamp(&start);

			for (;;) {
				uint32_t valid_channels = 0;
				for (i = 0; i < and_argc; i++) {
					int state;

					if (!peer_channels[i]) {
						continue;
					}

					state = switch_channel_get_state(peer_channels[i]);

					if (state < CS_HANGUP) {
						valid_channels++;
					} else {
						continue;
					}

					if (state >= CS_ROUTING) {
						goto endfor1;
					}

					if (caller_channel && !switch_channel_ready(caller_channel)) {
						goto notready;
					}

					if ((switch_timestamp(NULL) - start) > (time_t) timelimit_sec) {
						to++;
						idx = IDX_TIMEOUT;
						goto notready;
					}

					if (!sent_ring && !progress && progress_timelimit_sec && (switch_timestamp(NULL) - start) > (time_t) progress_timelimit_sec) {
						to++;
						idx = IDX_TIMEOUT;
						goto notready;
					}

					switch_yield(10000);
				}

				if (valid_channels == 0) {
					status = SWITCH_STATUS_GENERR;
					goto done;
				}

			}

		  endfor1:

			if (ringback_data && !switch_channel_test_flag(caller_channel, CF_ANSWERED)
				&& !switch_channel_test_flag(caller_channel, CF_EARLY_MEDIA)) {
				if ((status = switch_channel_pre_answer(caller_channel)) != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Media Establishment Failed.\n", switch_channel_get_name(caller_channel));
					goto done;
				}
			}

			if (session && (read_codec = switch_core_session_get_read_codec(session)) &&
				(ringback_data ||
				 (!(switch_channel_test_flag(caller_channel, CF_PROXY_MODE) && switch_channel_test_flag(caller_channel, CF_PROXY_MEDIA))))) {

				if (!(pass = (uint8_t) switch_test_flag(read_codec, SWITCH_CODEC_FLAG_PASSTHROUGH))) {
					if (switch_core_codec_init(&write_codec,
											   "L16",
											   NULL,
											   read_codec->implementation->actual_samples_per_second,
											   read_codec->implementation->microseconds_per_frame / 1000,
											   1, SWITCH_CODEC_FLAG_ENCODE | SWITCH_CODEC_FLAG_DECODE, NULL,
											   switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {


						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
										  "Raw Codec Activation Success L16@%uhz 1 channel %dms\n",
										  read_codec->implementation->actual_samples_per_second,
										  read_codec->implementation->microseconds_per_frame / 1000);
						write_frame.codec = &write_codec;
						write_frame.datalen = read_codec->implementation->bytes_per_frame;
						write_frame.samples = write_frame.datalen / 2;
						memset(write_frame.data, 255, write_frame.datalen);

						if (ringback_data) {
							char *tmp_data = NULL;

							switch_buffer_create_dynamic(&ringback.audio_buffer, 512, 1024, 0);
							switch_buffer_set_loops(ringback.audio_buffer, -1);

							if (switch_is_file_path(ringback_data)) {
								char *ext;

								if (strrchr(ringback_data, '.') || strstr(ringback_data, SWITCH_URL_SEPARATOR)) {
									switch_core_session_set_read_codec(session, &write_codec);
								} else {
									ringback.asis++;
									write_frame.codec = read_codec;
									ext = read_codec->implementation->iananame;
									tmp_data = switch_mprintf("%s.%s", ringback_data, ext);
									ringback_data = tmp_data;
								}

								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Play Ringback File [%s]\n", ringback_data);

								ringback.fhb.channels = read_codec->implementation->number_of_channels;
								ringback.fhb.samplerate = read_codec->implementation->actual_samples_per_second;
								if (switch_core_file_open(&ringback.fhb,
														  ringback_data,
														  read_codec->implementation->number_of_channels,
														  read_codec->implementation->actual_samples_per_second,
														  SWITCH_FILE_FLAG_READ | SWITCH_FILE_DATA_SHORT, NULL) != SWITCH_STATUS_SUCCESS) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Playing File\n");
									switch_safe_free(tmp_data);
									goto notready;
								}
								ringback.fh = &ringback.fhb;


							} else {
								teletone_init_session(&ringback.ts, 0, teletone_handler, &ringback);
								ringback.ts.rate = read_codec->implementation->actual_samples_per_second;
								switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Play Ringback Tone [%s]\n", ringback_data);
								//ringback.ts.debug = 1;
								//ringback.ts.debug_stream = switch_core_get_console();
								if (teletone_run(&ringback.ts, ringback_data)) {
									switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Error Playing Tone\n");
									teletone_destroy_session(&ringback.ts);
									switch_buffer_destroy(&ringback.audio_buffer);
									ringback_data = NULL;
								}
							}
							switch_safe_free(tmp_data);
						}
					} else {
						switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Codec Error!\n");
						switch_channel_hangup(caller_channel, SWITCH_CAUSE_NORMAL_TEMPORARY_FAILURE);
						read_codec = NULL;
					}
				}
			}

			if (ringback_data) {
				early_ok = 0;
			}

			while ((!caller_channel || switch_channel_ready(caller_channel)) &&
				   check_channel_status(peer_channels, peer_sessions, and_argc, &idx, &hups, file, key, early_ok, &ring_ready, &progress, return_ring_ready)) {
				
				if (caller_channel && !sent_ring && ring_ready && !return_ring_ready) {
					switch_channel_ring_ready(caller_channel);
					sent_ring = 1;
				}
				// When the AND operator is being used, and fail_on_single_reject is set, a hangup indicates that the call should fail.
				if ((to = (uint8_t) ((switch_timestamp(NULL) - start) >= (time_t) timelimit_sec))
					|| (fail_on_single_reject && hups)) {
					idx = IDX_TIMEOUT;
					goto notready;
				}

				if (peer_sessions[0]
					&& switch_core_session_dequeue_message(peer_sessions[0], &message) == SWITCH_STATUS_SUCCESS) {
					if (session && !ringback_data && or_argc == 1 && and_argc == 1) {	/* when there is only 1 channel to call and bridge and no ringback */
						switch_core_session_receive_message(session, message);
					}

					if (switch_test_flag(message, SCSMF_DYNAMIC)) {
						switch_safe_free(message);
					} else {
						message = NULL;
					}
				}

				/* read from the channel while we wait if the audio is up on it */
				if (session &&
					!switch_channel_test_flag(caller_channel, CF_PROXY_MODE) &&
					!switch_channel_test_flag(caller_channel, CF_PROXY_MEDIA) &&
					(ringback_data
					 || (switch_channel_test_flag(caller_channel, CF_ANSWERED) || switch_channel_test_flag(caller_channel, CF_EARLY_MEDIA)))) {

					switch_status_t tstatus = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);

					if (!SWITCH_READ_ACCEPTABLE(tstatus)) {
						break;
					}

					if (ring_ready && read_frame && !pass) {
						if (ringback.fh) {
							switch_size_t mlen, olen;
							unsigned int pos = 0;


							if (ringback.asis) {
								mlen = write_frame.codec->implementation->encoded_bytes_per_frame;
							} else {
								mlen = write_frame.codec->implementation->samples_per_frame;
							}

							olen = mlen;
							switch_core_file_read(ringback.fh, write_frame.data, &olen);

							if (olen == 0) {
								olen = mlen;
								ringback.fh->speed = 0;
								switch_core_file_seek(ringback.fh, &pos, 0, SEEK_SET);
								switch_core_file_read(ringback.fh, write_frame.data, &olen);
								if (olen == 0) {
									break;
								}
							}
							write_frame.datalen = (uint32_t) (ringback.asis ? olen : olen * 2);
						} else if (ringback.audio_buffer) {
							if ((write_frame.datalen = (uint32_t) switch_buffer_read_loop(ringback.audio_buffer,
																						  write_frame.data,
																						  write_frame.codec->implementation->bytes_per_frame)) <= 0) {
								break;
							}
						}

						if ((ringback.fh || ringback.audio_buffer) && write_frame.codec && write_frame.datalen) {
							if (switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0) != SWITCH_STATUS_SUCCESS) {
								break;
							}
						}
					}

				} else {
					switch_yield(1000);
				}

			}

		  notready:

			if (caller_channel && !switch_channel_ready(caller_channel)) {
				idx = IDX_CANCEL;
			}

			if (session && (ringback_data || !(switch_channel_test_flag(caller_channel, CF_PROXY_MODE) &&
											   switch_channel_test_flag(caller_channel, CF_PROXY_MEDIA)))) {
				switch_core_session_reset(session, SWITCH_FALSE);
			}

			for (i = 0; i < and_argc; i++) {
				if (!peer_channels[i]) {
					continue;
				}

				if (switch_channel_test_flag(peer_channels[i], CF_TRANSFER) || switch_channel_test_flag(peer_channels[i], CF_BRIDGED) ||
					switch_channel_get_state(peer_channels[i]) == CS_RESET || !switch_channel_test_flag(peer_channels[i], CF_ORIGINATING)
					) {
					continue;
				}

				if (i != idx) {
					const char *holding = NULL;

					if (idx == IDX_TIMEOUT || to) {
						reason = SWITCH_CAUSE_NO_ANSWER;
					} else {
						if (idx == IDX_CANCEL) {
							reason = SWITCH_CAUSE_ORIGINATOR_CANCEL;
						} else {
							if (and_argc > 1) {
								reason = SWITCH_CAUSE_LOSE_RACE;
							} else {
								reason = SWITCH_CAUSE_NO_ANSWER;
							}
						}
					}
					if (switch_channel_ready(peer_channels[i])) {
						if (caller_channel && i == 0) {
							holding = switch_channel_get_variable(caller_channel, SWITCH_HOLDING_UUID_VARIABLE);
							holding = switch_core_session_strdup(session, holding);
							switch_channel_set_variable(caller_channel, SWITCH_HOLDING_UUID_VARIABLE, NULL);
						}
						if (holding) {
							switch_ivr_uuid_bridge(holding, switch_core_session_get_uuid(peer_sessions[i]));
						} else {
							switch_channel_hangup(peer_channels[i], reason);
						}
					}
				}
			}


			if (idx > IDX_NADA) {
				peer_session = peer_sessions[idx];
				peer_channel = peer_channels[idx];
			} else {
				status = SWITCH_STATUS_FALSE;
				if (caller_channel && peer_channel) {
					process_import(session, peer_channel);
				}
				peer_channel = NULL;
				goto done;
			}

			if (caller_channel) {
				if (switch_channel_test_flag(peer_channel, CF_ANSWERED)) {
					status = switch_channel_answer(caller_channel);
				} else if (switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) {
					status = switch_channel_pre_answer(caller_channel);
				} else {
					status = SWITCH_STATUS_SUCCESS;
				}

				if (status != SWITCH_STATUS_SUCCESS) {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "%s Media Establishment Failed.\n", switch_channel_get_name(caller_channel));
					switch_channel_hangup(peer_channel, SWITCH_CAUSE_INCOMPATIBLE_DESTINATION);
				}
			}

			if (switch_channel_test_flag(peer_channel, CF_ANSWERED) ||
				(early_ok && switch_channel_test_flag(peer_channel, CF_EARLY_MEDIA)) ||
				(return_ring_ready && switch_channel_test_flag(peer_channel, CF_RING_READY))
				) {
				*bleg = peer_session;
				status = SWITCH_STATUS_SUCCESS;
			} else {
				status = SWITCH_STATUS_FALSE;
			}

		  done:
			
			*cause = SWITCH_CAUSE_NONE;

			if (caller_channel && !switch_channel_ready(caller_channel)) {
				status = SWITCH_STATUS_FALSE;
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				if (caller_channel) {
					switch_channel_set_variable(caller_channel, "originate_disposition", "call accepted");
					if (peer_channel) {
						process_import(session, peer_channel);
					}
				}
				switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Originate Resulted in Success: [%s]\n", switch_channel_get_name(peer_channel));
				*cause = SWITCH_CAUSE_SUCCESS;

			} else {
				const char *cdr_var = NULL;
				int cdr_total = 0;
				switch_xml_t cdr;
				char *xml_text;
				char buf[128] = "", buf2[128] = "";

				if (caller_channel) {
					cdr_var = switch_channel_get_variable(caller_channel, "failed_xml_cdr_prefix");
				}

				if (peer_channel) {
					*cause = switch_channel_get_cause(peer_channel);					
				} else {
					for (i = 0; i < and_argc; i++) {
						if (!peer_channels[i]) {
							continue;
						}
						*cause = switch_channel_get_cause(peer_channels[i]);
						break;
					}
				}
				
				if (cdr_var) {
					for (i = 0; i < and_argc; i++) {
						if (!peer_sessions[i]) {
                            continue;
                        }
						
						if (switch_ivr_generate_xml_cdr(peer_sessions[i], &cdr) == SWITCH_STATUS_SUCCESS) {
							if ((xml_text = switch_xml_toxml(cdr, SWITCH_FALSE))) {
								switch_snprintf(buf, sizeof(buf), "%s_%d", cdr_var, ++cdr_total);
								switch_channel_set_variable(caller_channel, buf, xml_text);
								switch_safe_free(xml_text);
							}
							switch_xml_free(cdr);
							cdr = NULL;
						}

					}
					switch_snprintf(buf, sizeof(buf), "%s_total", cdr_var);
					switch_snprintf(buf2, sizeof(buf2), "%d", cdr_total ? cdr_total : 0);
					switch_channel_set_variable(caller_channel, buf, buf2);
				}

				if (!*cause) {
					if (reason) {
						*cause = reason;
					} else if (caller_channel) {
						*cause = switch_channel_get_cause(caller_channel);
					} else {
						*cause = SWITCH_CAUSE_DESTINATION_OUT_OF_ORDER;
					}
				}

				if (*cause == SWITCH_CAUSE_SUCCESS || *cause == SWITCH_CAUSE_NONE) {
					*cause = SWITCH_CAUSE_ORIGINATOR_CANCEL;
				}

				if (idx == IDX_CANCEL) {
					*cause = SWITCH_CAUSE_ORIGINATOR_CANCEL;
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  "Originate Cancelled by originator termination Cause: %d [%s]\n", *cause, switch_channel_cause2str(*cause));

				} else {
					switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG,
									  "Originate Resulted in Error Cause: %d [%s]\n", *cause, switch_channel_cause2str(*cause));
				}
			}

			if (caller_channel) {
				switch_channel_set_variable(caller_channel, "originate_disposition", switch_channel_cause2str(*cause));
			}

			if (ringback.fh) {
				switch_core_file_close(ringback.fh);
				ringback.fh = NULL;
			} else if (ringback.audio_buffer) {
				teletone_destroy_session(&ringback.ts);
				switch_buffer_destroy(&ringback.audio_buffer);
			}

			switch_core_session_reset(session, SWITCH_FALSE);

			if (write_codec.implementation) {
				switch_core_codec_destroy(&write_codec);
			}

			for (i = 0; i < and_argc; i++) {
				if (!peer_channels[i]) {
					continue;
				}
				switch_channel_clear_flag(peer_channels[i], CF_ORIGINATING);
				if (status == SWITCH_STATUS_SUCCESS) { 
					if (bleg && *bleg && *bleg == peer_sessions[i]) {
						continue;
					}
				} else if (switch_channel_get_state(switch_core_session_get_channel(peer_sessions[i])) < CS_HANGUP) {
					switch_channel_hangup(switch_core_session_get_channel(peer_sessions[i]), *cause);
				}

				switch_core_session_rwunlock(peer_sessions[i]);
			}

			if (status == SWITCH_STATUS_SUCCESS) {
				goto outer_for;
			}
		}
	}
  outer_for:
	switch_safe_free(loop_data);
	switch_safe_free(odata);

	if (bleg && status != SWITCH_STATUS_SUCCESS) {
		*bleg = NULL;
	}


	if (var_event) {
		switch_event_destroy(&var_event);
	}
	switch_safe_free(write_frame.data);

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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
