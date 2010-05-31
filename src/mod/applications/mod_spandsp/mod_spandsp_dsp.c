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
 * The Original Code is FreeSWITCH mod_spandsp.
 *
 * The Initial Developer of the Original Code is
 * Massimo Cetra <devel@navynet.it>
 *
 * Portions created by the Initial Developer are Copyright (C)
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Brian West <brian@freeswitch.org>
 * Anthony Minessale II <anthm@freeswitch.org>
 * Steve Underwood <steveu@coppice.org>
 * Antonio Gallo <agx@linux.it>
 * mod_spandsp_dsp.c -- dsp applications provided by SpanDSP
 *
 */

#include "mod_spandsp.h"

typedef struct {
	switch_core_session_t *session;
	dtmf_rx_state_t *dtmf_detect;
	char last_digit;
	uint32_t samples;
	uint32_t last_digit_end;
	uint32_t digit_begin;
	uint32_t min_dup_digit_spacing;
} switch_inband_dtmf_t;

static void spandsp_dtmf_rx_realtime_callback(void *user_data, int code, int level, int delay)
{
	switch_inband_dtmf_t *pvt = (switch_inband_dtmf_t *)user_data;
	char digit = (char)code;
	if (digit) {
		/* prevent duplicate DTMF */
		if (digit != pvt->last_digit || (pvt->samples - pvt->last_digit_end) > pvt->min_dup_digit_spacing) {
			switch_dtmf_t dtmf;
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "DTMF BEGIN DETECTED: [%c]\n", digit);
			pvt->last_digit = digit;
			dtmf.digit = digit;
			dtmf.duration = switch_core_default_dtmf_duration(0);
			switch_channel_queue_dtmf(switch_core_session_get_channel(pvt->session), &dtmf);
			pvt->digit_begin = pvt->samples;
		} else {
			switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "DUP DTMF DETECTED: [%c]\n", digit);
			pvt->last_digit_end = pvt->samples;
		}
	} else {
		switch_log_printf(SWITCH_CHANNEL_SESSION_LOG(pvt->session), SWITCH_LOG_DEBUG, "DTMF END DETECTED: [%c], duration = %u ms\n", pvt->last_digit, (pvt->samples - pvt->digit_begin) / 8);
		pvt->last_digit_end = pvt->samples;
	}
}

static switch_bool_t inband_dtmf_callback(switch_media_bug_t *bug, void *user_data, switch_abc_type_t type)
{
	switch_inband_dtmf_t *pvt = (switch_inband_dtmf_t *) user_data;
	switch_frame_t *frame = NULL;
	switch_channel_t *channel = switch_core_session_get_channel(pvt->session);

	switch (type) {
	case SWITCH_ABC_TYPE_INIT: {
		const char *min_dup_digit_spacing_str = switch_channel_get_variable(channel, "min_dup_digit_spacing_ms");
		pvt->dtmf_detect = dtmf_rx_init(NULL, NULL, NULL);
		dtmf_rx_set_realtime_callback(pvt->dtmf_detect, spandsp_dtmf_rx_realtime_callback, pvt);
		if (!zstr(min_dup_digit_spacing_str)) {
			pvt->min_dup_digit_spacing = atoi(min_dup_digit_spacing_str) * 8;
		}
		break;
	}
	case SWITCH_ABC_TYPE_CLOSE:
		if (pvt->dtmf_detect) {
			dtmf_rx_free(pvt->dtmf_detect);
		}
		break;
	case SWITCH_ABC_TYPE_READ_REPLACE:
		if ((frame = switch_core_media_bug_get_read_replace_frame(bug))) {
			pvt->samples += frame->samples;
			dtmf_rx(pvt->dtmf_detect, frame->data, frame->samples);
			switch_core_media_bug_set_read_replace_frame(bug, frame);
		}
		break;
	case SWITCH_ABC_TYPE_WRITE:
	default:
		break;
	}

	return SWITCH_TRUE;
}

switch_status_t spandsp_stop_inband_dtmf_session(switch_core_session_t *session)
{
	switch_media_bug_t *bug;
	switch_channel_t *channel = switch_core_session_get_channel(session);

	if ((bug = switch_channel_get_private(channel, "dtmf"))) {
		switch_channel_set_private(channel, "dtmf", NULL);
		switch_core_media_bug_remove(session, &bug);
		return SWITCH_STATUS_SUCCESS;
	}
	return SWITCH_STATUS_FALSE;
}

switch_status_t spandsp_inband_dtmf_session(switch_core_session_t *session)
{
	switch_channel_t *channel = switch_core_session_get_channel(session);
	switch_media_bug_t *bug;
	switch_status_t status;
	switch_inband_dtmf_t *pvt;
	switch_codec_implementation_t read_impl = { 0 };

	switch_core_session_get_read_impl(session, &read_impl);

	if (!(pvt = switch_core_session_alloc(session, sizeof(*pvt)))) {
		return SWITCH_STATUS_MEMERR;
	}

   	pvt->session = session;


	if (switch_channel_pre_answer(channel) != SWITCH_STATUS_SUCCESS) {
		return SWITCH_STATUS_FALSE;
	}

	if ((status = switch_core_media_bug_add(session, "spandsp_dtmf_detect", NULL,
                                            inband_dtmf_callback, pvt, 0, SMBF_READ_REPLACE, &bug)) != SWITCH_STATUS_SUCCESS) {
		return status;
	}

	switch_channel_set_private(channel, "dtmf", bug);

	return SWITCH_STATUS_SUCCESS;
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
