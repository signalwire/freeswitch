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
	uint32_t ts = 0;
	int32_t timeout_ms = 0;
	struct {
		uint32_t processed_samples;
		uint32_t err_samples;
		uint32_t window_ms;
		uint32_t window_samples;
		int16_t sequence_sample;
		int16_t predicted_sample;
		float max_err;
		float max_err_hit;
		float max_err_ever;
		uint8_t in_sync;
		uint8_t hangup_on_error;
		switch_time_t timeout;
	} bert;

	channel = switch_core_session_get_channel(session);

	switch_channel_answer(channel);

	switch_core_session_get_read_impl(session, &read_impl);

	memset(&bert, 0, sizeof(bert));
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

	bert.timeout = (switch_micro_time_now() + (timeout_ms * 1000));

	write_frame.codec = switch_core_session_get_read_codec(session);
	write_frame.data = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);
	write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_DEBUG, "BERT Test Window=%ums/%u MaxErr=%f%%\n", bert.window_ms, bert.window_samples, bert.max_err);
	if (bert.window_samples <= 0) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failed to compute BERT window samples!\n");
		goto done;
	}
	while (switch_channel_ready(channel)) {
		int16_t *read_samples = NULL;
		int16_t *write_samples = NULL;
		status = switch_core_session_read_frame(session, &read_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}
		/* BERT Sync */
		read_samples = read_frame->data;
		write_samples = write_frame.data;
		for (i = 0; i < (read_frame->datalen / 2); i++) {
			if (bert.window_samples == bert.processed_samples) {
				/* Calculate error rate */
				float err = ((float)((float)bert.err_samples / (float)bert.processed_samples) * 100.0);
				if (err > bert.max_err) {
					if (bert.in_sync) {
						bert.in_sync = 0;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "BERT Sync Lost: %f%% loss\n", err);
						if (bert.hangup_on_error) {
							switch_channel_hangup(channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
						}
					}
				} else if (!bert.in_sync) {
					bert.in_sync = 1;
					synced = 1;
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_INFO, "BERT Sync\n");
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
				if (bert.timeout && !synced) {
					switch_time_t now = switch_micro_time_now();
					if (now >= bert.timeout) {
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "BERT Timeout\n");
						if (bert.hangup_on_error) {
							switch_channel_hangup(channel, SWITCH_CAUSE_MEDIA_TIMEOUT);
						}
					}
				}
			}
			if (bert.predicted_sample != read_samples[i]) {
				bert.err_samples++;
			}
			bert.predicted_sample = (read_samples[i] + 1);
			write_samples[i] = bert.sequence_sample++;
			bert.processed_samples++;
		}

		write_frame.datalen = read_frame->datalen;
		write_frame.samples = i;
		write_frame.timestamp = ts;
		status = switch_core_session_write_frame(session, &write_frame, SWITCH_IO_FLAG_NONE, 0);
		if (!SWITCH_READ_ACCEPTABLE(status)) {
			break;
		}
		ts += read_impl.samples_per_packet;
	}

done:
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
