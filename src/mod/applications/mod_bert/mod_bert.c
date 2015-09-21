/* 
 * FreeSWITCH Modular Media Switching Software Library / Soft-Switch Application
 * Copyright (C) 2005-2011, Anthony Minessale II <anthm@freeswitch.org>
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
 * The Original Code is FreeSWITCH Modular Media Switching Software Library / Oreka Recording Module
 *
 * The Initial Developer of the Original Code is
 * Moises Silva <moises.silva@gmail.com>
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Moises Silva <moises.silva@gmail.com>
 *
 * mod_bert -- Naive BERT tester
 *
 */

#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_bert_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_bert_shutdown);
SWITCH_MODULE_DEFINITION(mod_bert, mod_bert_load, mod_bert_shutdown, NULL);

#define G711_ULAW_IDLE_OCTET        0xFF

/* http://en.wikipedia.org/wiki/Digital_milliwatt */
unsigned char ulaw_digital_milliwatt[8] = { 0x1e, 0x0b, 0x0b, 0x1e, 0x9e, 0x8b, 0x8b, 0x9e };

typedef struct {
	uint32_t processed_samples;
	uint32_t err_samples;
	uint32_t window_ms;
	uint32_t window_samples;
	uint32_t stats_sync_lost_cnt;
	uint32_t stats_cng_cnt;
	uint8_t sequence_sample;
	uint8_t predicted_sample;
	float max_err;
	float max_err_hit;
	float max_err_ever;
	uint8_t in_sync;
	uint8_t hangup_on_error;
	uint8_t milliwatt_index;
	uint8_t milliwatt_prediction_index;
	switch_time_t timeout;
	FILE *input_debug_f;
	FILE *output_debug_f;
	switch_timer_t timer;
} bert_t;

#define bert_close_debug_streams(bert, session) \
	do { \
		int rc = 0; \
		if (bert.input_debug_f) { \
			rc = fclose(bert.input_debug_f); \
			if (rc) { \
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to close BERT input debug file!\n"); \
			} \
			bert.input_debug_f = NULL; \
		} \
		if (bert.output_debug_f) { \
			rc = fclose(bert.output_debug_f); \
			if (rc) { \
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to close BERT output debug file!\n"); \
			} \
			bert.output_debug_f = NULL; \
		} \
	} while (0);

#define BERT_STATS_VAR_SYNC_LOST "bert_stats_sync_lost"
#define BERT_STATS_VAR_SYNC_LOST_CNT "bert_stats_sync_lost_count"

#define BERT_EVENT_TIMEOUT "mod_bert::timeout"
#define BERT_EVENT_LOST_SYNC "mod_bert::lost_sync"
#define BERT_EVENT_IN_SYNC "mod_bert::in_sync"

#define BERT_DEFAULT_WINDOW_MS 1000
#define BERT_DEFAULT_MAX_ERR 10.0
#define BERT_DEFAULT_TIMEOUT_MS 10000
SWITCH_STANDARD_APP(bert_test_function)
{
	switch_status_t status;
	switch_frame_t *read_frame = NULL, write_frame = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	switch_channel_t *channel = NULL;
	switch_event_t *event = NULL;
	const char *var = NULL;
	int i = 0;
	int synced = 0;
	int32_t timeout_ms = 0;
	int32_t interval = 20;
	int32_t samples = 0;
	uint8_t *write_samples = NULL;
	uint8_t *m = NULL;
	const char *timer_name = NULL;
	switch_bool_t clean_frame = SWITCH_FALSE;
	bert_t bert = { 0 };

	channel = switch_core_session_get_channel(session);

	switch_channel_answer(channel);

	switch_core_session_get_read_impl(session, &read_impl);

	if (read_impl.ianacode != 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "This application only works when using ulaw codec\n");
		goto done;
	}

	bert.window_ms = BERT_DEFAULT_WINDOW_MS;
	bert.window_samples = switch_samples_per_packet(read_impl.samples_per_second, bert.window_ms);
	bert.max_err = BERT_DEFAULT_MAX_ERR;
	timeout_ms = BERT_DEFAULT_TIMEOUT_MS;

	/* check if there are user-defined overrides */
	if ((var = switch_channel_get_variable(channel, "bert_window_ms"))) {
		int tmp = atoi(var);
		if (tmp > 0) {
			bert.window_ms = tmp;
			bert.window_samples = switch_samples_per_packet(read_impl.samples_per_second, bert.window_ms);
		}
	}
	if ((var = switch_channel_get_variable(channel, "bert_timeout_ms"))) {
		int tmp = atoi(var);
		if (tmp > 0) {
			timeout_ms = tmp;
		}
	}
	if ((var = switch_channel_get_variable(channel, "bert_max_err"))) {
		double tmp = atoi(var);
		if (tmp > 0) {
			bert.max_err = (float)tmp;
		}
	}
	if ((var = switch_channel_get_variable(channel, "bert_hangup_on_error"))) {
		if (switch_true(var)) {
			bert.hangup_on_error = 1;
		}
	}
	if ((var = switch_channel_get_variable(channel, "bert_debug_io_file"))) {
		char debug_file[1024];
		snprintf(debug_file, sizeof(debug_file), "%s.in", var);
		bert.input_debug_f = fopen(debug_file, "w");
		if (!bert.input_debug_f) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open input debug file %s\n", debug_file);
		}
		snprintf(debug_file, sizeof(debug_file), "%s.out", var);
		bert.output_debug_f = fopen(debug_file, "w");
		if (!bert.output_debug_f) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to open output debug file %s\n", debug_file);
		}
	}
	if ((var = switch_channel_get_variable(channel, "bert_timer_name"))) {
		timer_name = var;
	}

	/* Setup the timer, so we can send audio at correct time frames even if we do not receive audio */
	if (timer_name) {
		interval = read_impl.microseconds_per_packet / 1000;
		samples = switch_samples_per_packet(read_impl.samples_per_second, interval);
		if (switch_core_timer_init(&bert.timer, timer_name, interval, samples, switch_core_session_get_pool(session)) == SWITCH_STATUS_SUCCESS) {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_DEBUG, "Setup timer success interval: %u  samples: %u\n", interval, samples);
		} else {
			switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Timer Setup Failed.  BERT cannot start!\n");
			goto done;
		}
		switch_core_timer_sync(&bert.timer);
	}

	bert.timeout = (switch_micro_time_now() + (timeout_ms * 1000));

	write_frame.codec = switch_core_session_get_read_codec(session);
	write_frame.data = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);
	write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "BERT Test Window=%ums/%u, MaxErr=%f%%, Timeout=%dms\n", bert.window_ms, bert.window_samples, bert.max_err, timeout_ms);
	if (bert.window_samples <= 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to compute BERT window samples!\n");
		goto done;
	}
	switch_channel_set_variable(channel, BERT_STATS_VAR_SYNC_LOST_CNT, "0");
	switch_channel_set_variable(channel, BERT_STATS_VAR_SYNC_LOST, "false");
	write_samples = write_frame.data;

	/* Prepare the buffer we'll keep sending over and over */
	for (i = 0; i < read_impl.samples_per_packet; i += sizeof(ulaw_digital_milliwatt)) {
		memcpy(&write_samples[i], ulaw_digital_milliwatt, sizeof(ulaw_digital_milliwatt));
	}

	write_frame.datalen = read_impl.encoded_bytes_per_packet;
	write_frame.samples = read_impl.samples_per_packet;

	for (;;) {

		if (!switch_channel_ready(channel)) {
			break;
		}

		switch_ivr_parse_all_events(session);

		if (timer_name) {
			if (switch_core_timer_next(&bert.timer) != SWITCH_STATUS_SUCCESS) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to step on timer!\n");
				break;
			}
			/* the playback() app does not set write_frame.timestamp unless a timer is used, what's the catch? does it matter? */
			write_frame.timestamp = bert.timer.samplecount;
		}

		if (bert.output_debug_f) {
			fwrite(write_frame.data, write_frame.datalen, 1, bert.output_debug_f);
		}
		status = switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		/* Proceed to read and process the received frame ...
		 * Note that switch_core_session_read_frame is a blocking operation, we could do reathing in another thread like the playback() app
		 * does using switch_core_service_session() but OTOH that would lead to more load/cpu usage, extra threads being launched per call leg 
		 * and most likely reduce the overall capacity of the test system */
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (bert.timeout && !synced) {
			switch_time_t now = switch_micro_time_now();
			if (now >= bert.timeout) {
				bert.timeout = 0;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "BERT Timeout (read_samples=%d, read_bytes=%d, expected_samples=%d, session=%s)\n",
						read_frame->samples, read_frame->datalen, read_impl.samples_per_packet, switch_core_session_get_uuid(session));
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, BERT_EVENT_TIMEOUT) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_basic_data(channel, event);
					switch_event_fire(&event);
				}
				if (bert.hangup_on_error) {
					switch_channel_hangup(channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
				}
			}
		}

		/* Treat CNG as silence */
		if (switch_test_flag(read_frame, SFF_CNG)) {
			read_frame->samples = read_impl.samples_per_packet;
			read_frame->datalen = read_impl.samples_per_packet;
			memset(read_frame->data, G711_ULAW_IDLE_OCTET, read_frame->datalen);
			bert.stats_cng_cnt++;
		}

		if (read_frame->samples != read_impl.samples_per_packet || !read_frame->datalen) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Read %d samples, expected %d!\n", read_frame->samples, read_impl.samples_per_packet);
			continue;
		}

		if (bert.input_debug_f) {
			size_t ret = fwrite(read_frame->data, read_frame->datalen, 1, bert.input_debug_f);
			if (ret != 1) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to write to BERT input debug file!\n");
			}
		}

		if (bert.window_samples == bert.processed_samples) {
			/* BERT err rate calculation */
			float err = 0.0;
			/* If the channel is going down, then it is expected we'll have errors, ignore them and bail out */
			if (!switch_channel_ready(channel)) {
				bert_close_debug_streams(bert, session);
				break;
			}
			/* Calculate error rate */
			err = ((float)((float)bert.err_samples / (float)bert.processed_samples) * 100.0);
			if (err > bert.max_err) {
				if (bert.in_sync) {
					bert.in_sync = 0;
					bert.stats_sync_lost_cnt++;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "BERT Sync Lost: %f%% loss (count=%u, cng_count=%d, err_samples=%u, session=%s)\n",
							err, bert.stats_sync_lost_cnt, bert.stats_cng_cnt, bert.err_samples, switch_core_session_get_uuid(session));
					switch_channel_set_variable_printf(channel, BERT_STATS_VAR_SYNC_LOST_CNT, "%u", bert.stats_sync_lost_cnt);
					switch_channel_set_variable(channel, BERT_STATS_VAR_SYNC_LOST, "true");
					if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, BERT_EVENT_LOST_SYNC) == SWITCH_STATUS_SUCCESS) {
						switch_channel_event_set_basic_data(channel, event);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "sync_lost_percent", "%f", err);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "sync_lost_count", "%u", bert.stats_sync_lost_cnt);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "cng_count", "%u", bert.stats_cng_cnt);
						switch_event_add_header(event, SWITCH_STACK_BOTTOM, "err_samples", "%u", bert.err_samples);
						switch_event_fire(&event);
					}
					if (bert.hangup_on_error) {
						switch_channel_hangup(channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
						bert_close_debug_streams(bert, session);
					}
				}
			} else if (!bert.in_sync) {
				bert.in_sync = 1;
				synced = 1;
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "BERT Sync Success\n");
				bert.stats_cng_cnt = 0;
				bert.timeout = 0;
				if (switch_event_create_subclass(&event, SWITCH_EVENT_CUSTOM, BERT_EVENT_IN_SYNC) == SWITCH_STATUS_SUCCESS) {
					switch_channel_event_set_basic_data(channel, event);
					switch_event_fire(&event);
				}
			}
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG10, "Err=%f%% (%u/%u)\n", err, bert.err_samples, bert.processed_samples);
			if (synced && err > bert.max_err_hit) {
				bert.max_err_hit = err;
			}
			if (err > bert.max_err_ever) {
				bert.max_err_ever = err;
			}
			bert.processed_samples = 0;
			bert.err_samples = 0;
		}

		if ((bert.in_sync || clean_frame) &&
		   !memcmp(read_frame->data, write_frame.data, read_frame->datalen)) {
			goto sync_check_done;
		}

		/* We're not in sync, or we might be going out of sync, find the start of the pattern ... */
		m = memmem(read_frame->data, read_frame->datalen, ulaw_digital_milliwatt, sizeof(ulaw_digital_milliwatt));
		if (m) {
			/* At least some bytes matched, let's find out the err sample count (could be zero if we're lucky and the whole frame matches) */
			uint8_t *end = NULL;
			size_t left = 0;
			int cerrs = bert.err_samples;
			/* Pattern found at least once in the frame, let's check if the rest of the frame also matches */
			m += sizeof(ulaw_digital_milliwatt);
			end = (uint8_t *)read_frame->data + read_frame->datalen;
			left = (size_t)(end - m);
			if (left && !memcmp(m, write_frame.data, left)) {
				bert.err_samples += (m - (uint8_t *)read_frame->data - sizeof(ulaw_digital_milliwatt));
			} else if (left) {
				int s = 0;
				bert.err_samples += (m - (uint8_t *)read_frame->data - sizeof(ulaw_digital_milliwatt));
				/* count error samples */
				for (s = 0; m != end; m++, s++) {
					if (ulaw_digital_milliwatt[s%8] != *m) {
						bert.err_samples++;
					}
				}
			}
			clean_frame = (cerrs == bert.err_samples) ? SWITCH_TRUE : SWITCH_FALSE;
		} else {
			/* the patter was not found in the whole frame, then the whole frame is out of sync */
			bert.err_samples += read_frame->samples;
			clean_frame = SWITCH_FALSE;
		}

sync_check_done:
		bert.processed_samples += read_frame->samples;
	}

done:
	bert_close_debug_streams(bert, session);
	if (bert.timer.interval) {
		switch_core_timer_destroy(&bert.timer);
	}
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "BERT Test Completed. MaxErr=%f%%\n", synced ? bert.max_err_hit : bert.max_err_ever);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_bert_load)
{
	switch_application_interface_t *app_interface = NULL;

	if (switch_event_reserve_subclass(BERT_EVENT_TIMEOUT) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", BERT_EVENT_TIMEOUT);
		return SWITCH_STATUS_TERM;
	}
	

	if (switch_event_reserve_subclass(BERT_EVENT_LOST_SYNC) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", BERT_EVENT_LOST_SYNC);
		return SWITCH_STATUS_TERM;
	}
	
	
	if (switch_event_reserve_subclass(BERT_EVENT_IN_SYNC) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_LOG, SWITCH_LOG_ERROR, "Couldn't register subclass %s!\n", BERT_EVENT_IN_SYNC);
		return SWITCH_STATUS_TERM;
	}
	
	
	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "bert_test", "Start BERT Test", "Start BERT Test", bert_test_function, "", SAF_NONE); 
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_bert_shutdown)
{
	switch_event_free_subclass(BERT_EVENT_TIMEOUT);
	switch_event_free_subclass(BERT_EVENT_LOST_SYNC);
	switch_event_free_subclass(BERT_EVENT_IN_SYNC);
	
	return SWITCH_STATUS_UNLOAD;
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
