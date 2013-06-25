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
 * mod_stress.cpp -- Detect Voice Stress
 *
 */

#include <stdexcept>
#include <stdio.h>
#include "FFTReal.h"
using namespace std;

#include <switch.h>

SWITCH_MODULE_LOAD_FUNCTION(mod_stress_load);
SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_stress_shutdown);
SWITCH_MODULE_DEFINITION(mod_stress, mod_stress_load, mod_stress_shutdown, NULL);

struct stress_helper {
	switch_core_session_t *session;
	int read;
	uint32_t frame_size;
    FFTReal *fft;
    float *data;
    float *result;
    float *pow_spectrum;
    float bind;
    int start;
    int end;
    float avg_tremor_pwr;
    float avg_total_pwr;
    float total_pwr;
    float tremor_ratio;
    float stress;
    uint32_t rate;
    switch_buffer_t *audio_buffer;
    int16_t *audio;
};

static switch_bool_t stress_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	struct stress_helper *sth = (struct stress_helper *) user_data;

	switch (type) {
	case SWITCH_ABC_TYPE_INIT:
		{
			switch_codec_t *read_codec = switch_core_session_get_read_codec(sth->session);

            sth->rate = read_codec->implementation->actual_samples_per_second;

            if (sth->rate == 8000) {
                sth->frame_size = 8192;
            } else if (sth->rate == 16000) {
                sth->frame_size = 16384;
            } else if (sth->rate == 32000) {
                sth->frame_size = 32768;
            } else {
                return SWITCH_FALSE;
            }
            
            sth->data = (float *) switch_core_session_alloc(sth->session, sizeof(*sth->data) * sth->frame_size);
            sth->result = (float *) switch_core_session_alloc(sth->session, sizeof(*sth->result) * sth->frame_size);
            sth->pow_spectrum = (float *) switch_core_session_alloc(sth->session, sizeof(*sth->pow_spectrum) * sth->frame_size);
            sth->audio = (int16_t *) switch_core_session_alloc(sth->session, sizeof(*sth->audio) * sth->frame_size);
            
            sth->fft = new FFTReal (sth->frame_size);
            switch_buffer_create_dynamic(&sth->audio_buffer, sth->frame_size, sth->frame_size * 3, 0);

            sth->bind = (float) sth->rate / sth->frame_size;
            sth->start = (int) (8.0 / sth->bind);
            sth->end = (int) (14.0 / sth->bind);
            
		}
		break;
	case SWITCH_ABC_TYPE_CLOSE:
		{
            switch_buffer_destroy(&sth->audio_buffer);
            delete sth->fft;
		}
		break;
	case SWITCH_ABC_TYPE_READ:
	case SWITCH_ABC_TYPE_WRITE:
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
	case SWITCH_ABC_TYPE_WRITE_REPLACE:
		{
			switch_frame_t *frame;
            
			if (sth->read) {
				frame = switch_core_media_bug_get_read_replace_frame(bug);
			} else {
				frame = switch_core_media_bug_get_write_replace_frame(bug);
			}

            if (!switch_test_flag(frame, SFF_CNG)) {
                switch_buffer_write(sth->audio_buffer, frame->data, frame->datalen);
            }
            
            sth->stress = 0.0;

            if (switch_buffer_inuse(sth->audio_buffer) >= sth->frame_size * sizeof(int16_t)) {
                switch_size_t bytes;
                uint32_t samples, i;
                const float threshold = 1.5;

                bytes = switch_buffer_read(sth->audio_buffer, sth->audio, sth->frame_size * sizeof(int16_t));
                samples = bytes / sizeof(int16_t);
                
                switch_short_to_float(sth->audio, sth->data, samples);
                sth->fft->do_fft(sth->result, sth->data);

                for (i = 0; i < samples; ++i) {
                    sth->pow_spectrum[i] = pow(fabs(sth->result[i]), 2) / (float) samples;
                }

                sth->avg_tremor_pwr = 0.0;
                sth->avg_total_pwr = 0.0;
                sth->total_pwr = 0.0;
                
                for (i = sth->start; i <= sth->end; ++i) {
                    sth->avg_tremor_pwr += sth->pow_spectrum[i];
                }
                sth->avg_tremor_pwr /= ((sth->end - sth->start) + 1);

                for (i = 0; i < samples; ++i) {
                    sth->total_pwr += sth->pow_spectrum[i];
                }
                sth->avg_total_pwr = sth->total_pwr / samples;
                
                if (sth->total_pwr < threshold) {
                    sth->tremor_ratio = 0.0;
                } else {
                    sth->tremor_ratio = sth->avg_tremor_pwr / sth->avg_total_pwr;
                }

                if (sth->total_pwr >= 1.0) {
                    float d = pow(sth->tremor_ratio, 4);
                    if (d > 0.0) {
                        sth->stress = (10.0 / d) / 10000;
                        if (sth->stress >= 20000.0) {
                            sth->stress = 20000.0;
                        }
                    }
                }
            }

            if (sth->stress) {
                switch_event_t *event, *dup;
                switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_DEBUG, "Stress %0.2f\n", sth->stress);

                if (switch_event_create(&event, SWITCH_EVENT_DETECTED_SPEECH) == SWITCH_STATUS_SUCCESS) {
                    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Speech-Type", "stress-level");
                    switch_event_add_header(event, SWITCH_STACK_BOTTOM, "Stress-Level", "%0.2f", sth->stress);
		    switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "Unique-ID", switch_core_session_get_uuid(sth->session));
                    if (switch_event_dup(&dup, event) == SWITCH_STATUS_SUCCESS) {
                        switch_event_fire(&dup);
                    }
                    if (switch_core_session_queue_event(sth->session, &event) != SWITCH_STATUS_SUCCESS) {
                        switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(switch_core_media_bug_get_session(bug)), SWITCH_LOG_ERROR, "Event queue failed!\n");
                        switch_event_add_header_string(event, SWITCH_STACK_BOTTOM, "delivery-failure", "true");
                        switch_event_fire(&event);
                    }
                }
            }
		}
	default:
		break;
	}

	return SWITCH_TRUE;
}

SWITCH_STANDARD_APP(stress_start_function)
{
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_channel_t *channel = switch_core_session_get_channel(session);
	struct stress_helper *sth;
	char *argv[6];
	int argc;
	char *lbuf = NULL;
	int x = 0;

	if ((bug = (switch_media_bug_t *) switch_channel_get_private(channel, "_stress_"))) {
		if (!zstr(data) && !strcasecmp(data, "stop")) {
			switch_channel_set_private(channel, "_stress_", NULL);
			switch_core_media_bug_remove(session, &bug);
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_WARNING, "Cannot run 2 at once on the same channel!\n");
		}
		return;
	}

	sth = (struct stress_helper *) switch_core_session_alloc(session, sizeof(*sth));
	assert(sth != NULL);


	if (data && (lbuf = switch_core_session_strdup(session, data))
		&& (argc = switch_separate_string(lbuf, ' ', argv, (sizeof(argv) / sizeof(argv[0]))))) {
        if (!strncasecmp(argv[x], "read", 4)) {
            sth->read = 1;
        }
	}

	sth->session = session;
    
	if ((status = switch_core_media_bug_add(session, "stress", NULL, stress_callback, sth, 0,
											sth->read ? SMBF_READ_REPLACE : SMBF_WRITE_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(session), SWITCH_LOG_ERROR, "Failure!\n");
		return;
	}

	switch_channel_set_private(channel, "_stress_", bug);

}

SWITCH_MODULE_LOAD_FUNCTION(mod_stress_load)
{
    switch_application_interface_t *app_interface;

	*module_interface = switch_loadable_module_create_module_interface(pool, modname);

	SWITCH_ADD_APP(app_interface, "stress", "Analyze the stream for voice stress", "Analyze the stream for voice stress", 
                   stress_start_function, "[read|write|stop]", SAF_NONE);

	/* indicate that the module should continue to be loaded */
	return SWITCH_STATUS_SUCCESS;
}

SWITCH_MODULE_SHUTDOWN_FUNCTION(mod_stress_shutdown)
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
 * vim:set softtabstop=4 shiftwidth=4 tabstop=4 noet:
 */
