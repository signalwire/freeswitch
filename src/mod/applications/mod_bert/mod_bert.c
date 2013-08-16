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

/* http://en.wikipedia.org/wiki/Digital_milliwatt */
unsigned char ulaw_digital_milliwatt[8] = { 0x1e, 0x0b, 0x0b, 0x1e, 0x9e, 0x8b, 0x8b, 0x9e };

typedef struct {
	uint32_t processed_samples;
	uint32_t err_samples;
	uint32_t window_ms;
	uint32_t window_samples;
	uint32_t stats_sync_lost_cnt;
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
} bert_t;

#define bert_increase_milliwatt_index(index) \
	do { \
		if ((index) == (switch_arraylen(ulaw_digital_milliwatt)-1)) { \
			(index) = 0; \
		} else { \
			(index) = ((index) + 1); \
		} \
	} while (0);

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

#define BERT_DEFAULT_WINDOW_MS 1000
#define BERT_DEFAULT_MAX_ERR 10.0
#define BERT_DEFAULT_TIMEOUT_MS 10000
SWITCH_STANDARD_APP(bert_test_function)
{
	switch_status_t status;
	switch_frame_t *read_frame = NULL, write_frame = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	switch_channel_t *channel = NULL;
	const char *var = NULL;
	int i = 0;
	int synced = 0;
	uint32_t write_ts = 0;
	int32_t timeout_ms = 0;
	bert_t bert = { 0 };

	memset(&bert, 0, sizeof(bert));

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
	while (switch_channel_ready(channel)) {
		uint8_t *read_samples = NULL;
		uint8_t *write_samples = NULL;

		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		if (bert.timeout && !synced) {
			switch_time_t now = switch_micro_time_now();
			if (now >= bert.timeout) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "BERT Timeout (read_samples=%d, read_bytes=%d, expected_samples=%d, session=%s)\n",
						read_frame->samples, read_frame->datalen, read_impl.samples_per_packet, switch_core_session_get_uuid(session));
				if (bert.hangup_on_error) {
					switch_channel_hangup(channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
				}
			}
		}

		/* Ignore confort noise, TODO: we should probably deal with this and treat it as a full frame of silence?? */
		if (switch_test_flag(read_frame, SFF_CNG) || !read_frame->datalen) {
			continue;
		}

		if (read_frame->samples != read_impl.samples_per_packet) {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Only read %d samples, expected %d!\n", read_frame->samples, read_impl.samples_per_packet);
			continue;
		}

		read_samples = read_frame->data;
		write_samples = write_frame.data;
		if (bert.input_debug_f) {
			size_t ret = fwrite(read_frame->data, read_frame->datalen, 1, bert.input_debug_f);
			if (ret != 1) {
				switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to write to BERT input debug file!\n");
			}
		}

		/* BERT Sync Loop */
		for (i = 0; i < read_frame->samples; i++) {
			if (bert.window_samples == bert.processed_samples) {
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
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "BERT Sync Lost: %f%% loss (count=%u, err_samples=%u, session=%s)\n",
								err, bert.stats_sync_lost_cnt, bert.err_samples, switch_core_session_get_uuid(session));
						switch_channel_set_variable_printf(channel, BERT_STATS_VAR_SYNC_LOST_CNT, "%u", bert.stats_sync_lost_cnt);
						switch_channel_set_variable(channel, BERT_STATS_VAR_SYNC_LOST, "true");
						if (bert.hangup_on_error) {
							switch_channel_hangup(channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
							bert_close_debug_streams(bert, session);
						}
					}
				} else if (!bert.in_sync) {
					bert.in_sync = 1;
					synced = 1;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "BERT Sync Success\n");
					bert.timeout = 0;
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

			if (bert.predicted_sample != read_samples[i]) {
				bert.err_samples++;
				if (!bert.in_sync) {
					/* If we're not in sync, we must reset the index on error to start the pattern detection again */
					bert.milliwatt_prediction_index = 0;
				}
			}

			/* Calculate our next sequence sample to write */
			bert.sequence_sample = ulaw_digital_milliwatt[bert.milliwatt_index];
			//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[%d] 0x%X\n", bert.milliwatt_index, bert.sequence_sample);
			bert_increase_milliwatt_index(bert.milliwatt_index);
			//switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "[%d] 0x%X\n", bert.milliwatt_index, bert.sequence_sample);
			write_samples[i] = bert.sequence_sample;

			/* Try to guess what the next sample will be in the milliwatt sequence */
			bert.predicted_sample = ulaw_digital_milliwatt[bert.milliwatt_prediction_index];
			bert_increase_milliwatt_index(bert.milliwatt_prediction_index);

			bert.processed_samples++;
		}

		write_frame.datalen = read_frame->datalen;
		write_frame.samples = i;
		write_frame.timestamp = write_ts;
		if (bert.output_debug_f) {
			fwrite(write_frame.data, write_frame.datalen, 1, bert.output_debug_f);
		}
		status = switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}

		write_ts += read_impl.samples_per_packet;
	}

done:
	bert_close_debug_streams(bert, session);
	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "BERT Test Completed. MaxErr=%f%%\n", synced ? bert.max_err_hit : bert.max_err_ever);
}

SWITCH_MODULE_LOAD_FUNCTION(mod_bert_load)
{
	switch_application_interface_t *app_interface = NULL;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "bert_test", "Start BERT Test", "Start BERT Test", bert_test_function, "", SAF_NONE); 
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_bert_shutdown)
{
	return SWITCH_STATUS_UNLOAD;
}

/* For Emacs:
 * Local Variables:
 * mode:c
 * indent-tabs-mode:nil
 * tab-width:4
 * c-basic-offset:4
 * End:
 * For VIM:
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4:
 */
