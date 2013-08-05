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
 * mod_g711_bert -- Naive BERT tester
 *
 */

#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_bert_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_bert_shutdown);
SWITCH_MODULE_DEFINITION(mod_bert, mod_bert_load, mod_bert_shutdown, NULL);

SWITCH_STANDARD_APP(g711_bert_function)
{
	switch_status_t status;
	switch_frame_t *read_frame = NULL, write_frame = { 0 };
	switch_codec_implementation_t read_impl = { 0 };
	switch_channel_t *channel = NULL;
	int i = 0;
	uint32_t interval = 0;
	uint32_t ts = 0;
	struct {
		uint64_t sync;
		uint32_t bytes_since_sync;
		uint32_t min_sync_samples;
		uint32_t max_sync_err;
		uint32_t sync_err;
		int16_t tx_sample;
		int16_t test_data;
	} bert;

	memset(&bert, 0, sizeof(bert));
	channel = switch_core_session_get_channel(session);

	switch_channel_answer(channel);

	switch_core_session_get_read_impl(session, &read_impl);

	interval = read_impl.microseconds_per_packet / 1000;

	write_frame.codec = switch_core_session_get_read_codec(session);
	write_frame.data = switch_core_session_alloc(session, SWITCH_RECOMMENDED_BUFFER_SIZE);
	write_frame.buflen = SWITCH_RECOMMENDED_BUFFER_SIZE;

	bert.min_sync_samples = (read_impl.samples_per_packet * 40);
	bert.max_sync_err = (read_impl.samples_per_packet * 20);
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
			if (bert.sync < bert.min_sync_samples) {
				if (bert.test_data == read_samples[i]) {
					bert.sync++;
				}
			} else {
				if (bert.sync == bert.min_sync_samples) {
					switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "G.711 synced (%lu)\n", bert.sync);
					bert.bytes_since_sync = 0;
					bert.sync_err = 0;
				}
				bert.bytes_since_sync++;
				if (bert.test_data != read_samples[i]) {
					bert.sync_err++;
					if (bert.sync_err >= bert.max_sync_err) {
						bert.sync = 0;
						switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "G.711 sync lost (%u)\n", bert.bytes_since_sync);
					}
				} else {
					bert.sync++;
				}
			}
			bert.test_data = (read_samples[i] + 1);

			write_samples[i] = bert.tx_sample++;
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
}

SWITCH_MODULE_LOAD_FUNCTION(mod_bert_load)
{
	switch_application_interface_t *app_interface = NULL;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "bert_test", "Start BERT Test", "Start BERT Test", g711_bert_function, "", SAF_NONE); 
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
